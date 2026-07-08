/*
 hobd_esp - single-board Honda K-line -> WiFi/WebSocket dashboard

 Role: the ESP32 now reads the Honda K-line DIRECTLY (no ATmega). It:
 - drives the 3-pin DLC K-line (Honda pre-2002 datalogger, 9600 8N1, single-wire
   half-duplex) through an ST L9637D transceiver on hardware UART2,
 - decodes the ECU rows into sensor values (OBD1, B16 target),
 - broadcasts one compact JSON frame (~4 Hz) to all WebSocket clients, and
 - serves the PWA dashboard (dashboard/) from LittleFS over HTTP,
 - runs as a WiFi Access Point so it works in a car with no router.

 Supersedes the old two-board design (ATmega hobd_uni + ESP bridge). Bluetooth/
 Torque (ELM327) and the LCD are dropped.

 Board: ESP32 dev board (WROOM). Tools > Board > "ESP32 Dev Module".

 Libraries (maintained ESP32Async forks; the older ESPAsyncWebServer/AsyncTCP
 forks fail to build on ESP32 core 3.x):
 - "WebSockets"          by Markus Sattler (Links2004/arduinoWebSockets)
 - "ESP Async WebServer" by ESP32Async
 - "Async TCP"           by ESP32Async
 - WiFi, LittleFS, FS    (bundled with the ESP32 core)

 Wiring (K-line through the L9637D transceiver, VCC referenced to 3V3 so RX/TX are
 native 3.3V -- no divider):
 - L9637D VCC(7) -> ESP32 3V3    VS(6) -> protected +12V    GND(4) -> GND
 - L9637D K(2) --[510R]--> DLC K-line     LI(5) -> GND     LO(3) open
 - L9637D TX(1) <- ESP32 GPIO17 (UART2 TX)
 - L9637D RX(8) -> ESP32 GPIO16 (UART2 RX)   (ESP RX is NOT 5V tolerant)
 - Power the ESP32 from a 12V->5V buck (LM2596) with SS54 + SMBJ24A protection.
 Upload the dashboard files to LittleFS (tools/deploy-esp.sh).
*/

#include <WiFi.h>
#include <WebSocketsServer.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>

// ---- config ----
const char *AP_SSID = "HondaOBD";
const char *AP_PASS = "honda1234"; // >= 8 chars; change me
const uint8_t KLINE_RX_PIN = 16;   // ESP32 UART2 RX <- L9637D RX(8)
const uint8_t KLINE_TX_PIN = 17;   // ESP32 UART2 TX -> L9637D TX(1)
const uint32_t KLINE_BAUD = 9600;  // Honda K-line, 8N1
const byte obd_select = 1;         // 1 = OBD1 (B16 target). RPM formula switch.

// The K-line is single-wire half-duplex: through the transceiver, our own 5 request
// bytes echo back on RX before the ECU reply. Set 0 only for a rare transceiver that
// tri-states RX during TX (won't echo).
#define KLINE_ECHO 1

HardwareSerial &dlcSerial = Serial2; // K-line UART
WebSocketsServer webSocket(81);      // ws://<ip>:81
AsyncWebServer httpServer(80);       // http://<ip>/  (serves the PWA)

// ---- K-line state ----
byte dlcdata[20] = {0};                    // dlc reply buffer
byte dlcTimeout = 0, dlcChecksumError = 0; // link-health counters (saturate at 255)
int dtcErrors[10];
byte dtcCount = 0;

// ---- shared snapshot (K-line task writes, loop reads) ----
struct Snapshot {
  int rpm, ect, iat, maps, tps, volt, sft, lft, inj, ign, iac, knoc;
  byte vss, vssavg, vsstop, mil;
  int et, ec;
  int dtc[10];
  byte dtcN;
};
Snapshot snap = {0};
SemaphoreHandle_t snapMutex;

// --- K-line transport ---
void dlcInit() {
  dlcSerial.write(0x68);
  dlcSerial.write(0x6a);
  dlcSerial.write(0xf5);
  dlcSerial.write(0xaf);
  dlcSerial.write(0xbf);
  dlcSerial.write(0xb3);
  dlcSerial.write(0xb2);
  dlcSerial.write(0xc1);
  dlcSerial.write(0xdb);
  dlcSerial.write(0xb3);
  dlcSerial.write(0xe9);
  delay(300);
}

int dlcCommand(byte cmd, byte num, byte loc, byte len) {
  byte crc = (0xFF - (cmd + num + loc + len - 0x01)); // Honda checksum
  unsigned long timeOut = millis() + 200;             // timeout @ 200 ms

  memset(dlcdata, 0, sizeof(dlcdata));
  while (dlcSerial.available()) dlcSerial.read(); // drop stale bytes

  dlcSerial.write(cmd); // read-memory cmd
  dlcSerial.write(num); // num of bytes to send
  dlcSerial.write(loc); // address
  dlcSerial.write(len); // num of bytes to read
  dlcSerial.write(crc); // checksum
  dlcSerial.flush();    // block until the 5 bytes have left the UART

#if KLINE_ECHO
  // half-duplex: our own 5 bytes come back on RX. discard them before the reply.
  int echo = 0;
  while (echo < 5 && millis() < timeOut) {
    if (dlcSerial.available()) { dlcSerial.read(); echo++; }
    else yield();
  }
  if (echo < 5) {
    if (dlcTimeout < 255) dlcTimeout++;
    return 0; // failed
  }
#endif

  // reply: 00 len+3 data...
  int i = 0;
  while (i < (len + 3) && millis() < timeOut) {
    if (dlcSerial.available()) { dlcdata[i] = dlcSerial.read(); i++; }
    else yield();
  }
  if (i < (len + 3)) { // timeout
    if (dlcTimeout < 255) dlcTimeout++;
    return 0; // failed
  }

  // checksum
  crc = 0;
  for (i = 0; i < len + 2; i++) crc = crc + dlcdata[i];
  crc = 0xFF - (crc - 1);
  if (crc != dlcdata[len + 2]) { // checksum failed
    if (dlcChecksumError < 255) dlcChecksumError++;
    return 0; // failed
  }

  return 1; // success
}

// Read DTC Error (Honda MIL code numbers) into dtcErrors[]/dtcCount
void scanDtc() {
  byte i;
  dtcCount = 0;
  if (dlcCommand(0x20, 0x05, 0x40, 0x10)) { // row 5
    for (i = 0; i < 14; i++) {
      if ((dlcdata[i + 2] >> 4) && dtcCount < 10) {
        dtcErrors[dtcCount] = i * 2;
        dtcCount++;
      }
      if ((dlcdata[i + 2] & 0xf) && dtcCount < 10) {
        dtcErrors[dtcCount] = (i * 2) + 1;
        // haxx (ECU quirk, keep exactly)
        if (dtcErrors[dtcCount] == 23) dtcErrors[dtcCount] = 22;
        if (dtcErrors[dtcCount] == 24) dtcErrors[dtcCount] = 23;
        dtcCount++;
      }
    }
  }
}

// K-line reader: pinned to core 1, reads the ECU every 250 ms, publishes a snapshot.
void klineTask(void *pv) {
  dlcInit();

  unsigned long dtcTick = 0;
  unsigned long vsssum = 0, running_time = 0;
  byte vsstop = 0, vssavg = 0;

  for (;;) {
    int rpm = 0, ect = 0, iat = 0, maps = 0, tps = 0, volt = 0;
    int sft = 0, lft = 0, inj = 0, ign = 0, iac = 0, knoc = 0;
    byte vss = 0;

    if (dlcCommand(0x20, 0x05, 0x00, 0x10)) { // row 1
      if (obd_select == 1) rpm = 1875000 / (dlcdata[2] * 256 + dlcdata[3] + 1); // OBD1
      else                 rpm = (dlcdata[2] * 256 + dlcdata[3]) / 4;           // OBD2
      if (rpm < 0) rpm = 0; // in obd1 rpm reads -1 at 0
      vss = dlcdata[4];
    }

    delay(1);
    if (dlcCommand(0x20, 0x05, 0x10, 0x10)) { // row 2
      float f;
      f = dlcdata[2];
      f = 155.04149 - f * 3.0414878 + pow(f, 2) * 0.03952185 - pow(f, 3) * 0.00029383913 + pow(f, 4) * 0.0000010792568 - pow(f, 5) * 0.0000000015618437;
      ect = round(f);
      f = dlcdata[3];
      f = 155.04149 - f * 3.0414878 + pow(f, 2) * 0.03952185 - pow(f, 3) * 0.00029383913 + pow(f, 4) * 0.0000010792568 - pow(f, 5) * 0.0000000015618437;
      iat = round(f);
      maps = dlcdata[4] * 0.716 - 5; // kPa
      tps = (dlcdata[6] - 24) / 2;
      f = dlcdata[9];
      f = f / 10.45;         // battery volts
      volt = round(f * 10);  // x10 deci-units (web scales back)
    }

    delay(1);
    if (dlcCommand(0x20, 0x05, 0x20, 0x10)) { // row 3
      float f;
      sft = (dlcdata[2] / 128 - 1) * 100; // -30..30 (integer division: ECU contract)
      lft = (dlcdata[3] / 128 - 1) * 100;
      inj = (dlcdata[6] * 256 + dlcdata[7]) / 250; // ms, 0..16
      f = dlcdata[8];
      f = (f - 24) / 4;
      ign = round(f * 10); // x10 deci-units (degrees)
      iac = dlcdata[10] / 2.55;
    }

    delay(1);
    if (dlcCommand(0x20, 0x05, 0x30, 0x10)) { // row 4
      knoc = dlcdata[14] / 51; // 0..5
    }

    // trip computer: top + average speed
    if (vss > vsstop) vsstop = vss;
    if (rpm > 0 && vss > 0) {
      running_time++;
      vsssum += vss;
      vssavg = vsssum / running_time;
    }

    // periodic DTC auto-scan (~10s) so the stream has fresh codes
    if (millis() - dtcTick >= 10000) {
      dtcTick = millis();
      scanDtc();
    }

    // publish snapshot
    xSemaphoreTake(snapMutex, portMAX_DELAY);
    snap.rpm = rpm; snap.vss = vss; snap.ect = ect; snap.iat = iat;
    snap.maps = maps; snap.tps = tps; snap.volt = volt; snap.sft = sft;
    snap.lft = lft; snap.inj = inj; snap.ign = ign; snap.iac = iac;
    snap.knoc = knoc; snap.vssavg = vssavg; snap.vsstop = vsstop;
    snap.et = dlcTimeout; snap.ec = dlcChecksumError;
    snap.mil = dtcCount > 0 ? 1 : 0;
    snap.dtcN = dtcCount;
    for (byte k = 0; k < dtcCount && k < 10; k++) snap.dtc[k] = dtcErrors[k];
    xSemaphoreGive(snapMutex);

    vTaskDelay(250 / portTICK_PERIOD_MS);
  }
}

void onWsEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t len) {
  // read-only stream: inbound messages are ignored (no pin-write API on the wire).
  if (type == WStype_CONNECTED) {
    Serial.printf("ws client %u connected\n", num);
  }
}

void setup() {
  Serial.begin(115200); // USB debug
  dlcSerial.begin(KLINE_BAUD, SERIAL_8N1, KLINE_RX_PIN, KLINE_TX_PIN);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP()); // 192.168.4.1

  webSocket.begin();
  webSocket.onEvent(onWsEvent);

  // do NOT auto-format on mount failure: the deploy script flashes a valid image, so
  // a mount failure means "not flashed yet" -- formatting would wipe the dashboard.
  if (LittleFS.begin(false)) {
    httpServer.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
    httpServer.onNotFound([](AsyncWebServerRequest *req) {
      req->send(LittleFS, "/index.html", "text/html");
    });
    httpServer.begin();
    Serial.println("HTTP server serving /dashboard from LittleFS");
  } else {
    Serial.println("LittleFS mount failed - run tools/deploy-esp.sh to flash the dashboard (WS still works)");
  }

  snapMutex = xSemaphoreCreateMutex();
  // K-line reader on core 1; WiFi/WS stack lives on core 0 (loop runs on core 1 too,
  // but the reader blocks/yield()s and only ~1 ms/row, so WS is not starved).
  xTaskCreatePinnedToCore(klineTask, "kline", 4096, NULL, 1, NULL, 1);
}

void loop() {
  webSocket.loop();

  static unsigned long tick = 0;
  if (millis() - tick >= 250) {
    tick = millis();

    Snapshot s;
    xSemaphoreTake(snapMutex, portMAX_DELAY);
    s = snap;
    xSemaphoreGive(snapMutex);

    char buf[340];
    int n = snprintf(buf, sizeof(buf),
      "{\"rpm\":%d,\"vss\":%d,\"ect\":%d,\"iat\":%d,\"map\":%d,\"tps\":%d,"
      "\"volt\":%d,\"sft\":%d,\"lft\":%d,\"inj\":%d,\"ign\":%d,\"iac\":%d,"
      "\"knoc\":%d,\"vavg\":%d,\"vtop\":%d,\"et\":%d,\"ec\":%d,\"mil\":%d,\"dtc\":[",
      s.rpm, s.vss, s.ect, s.iat, s.maps, s.tps,
      s.volt, s.sft, s.lft, s.inj, s.ign, s.iac,
      s.knoc, s.vssavg, s.vsstop, s.et, s.ec, s.mil);
    for (byte k = 0; k < s.dtcN && k < 10 && n < (int)sizeof(buf) - 8; k++)
      n += snprintf(buf + n, sizeof(buf) - n, "%s%d", k ? "," : "", s.dtc[k]);
    n += snprintf(buf + n, sizeof(buf) - n, "]}");

    webSocket.broadcastTXT(buf, n);
  }
}
