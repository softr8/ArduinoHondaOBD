/*
 hobd_esp - WiFi / WebSocket co-processor for ArduinoHondaOBD

 Role: bridge between the ATmega328 (hobd_uni / hobd_uni2) and any device.
 - Reads newline-delimited JSON frames from the ATmega over UART (~4 Hz).
 - Broadcasts each frame to all WebSocket clients (live dashboard).
 - Serves the PWA dashboard (dashboard/) from LittleFS over HTTP.
 - Runs as a WiFi Access Point so it works in a car with no router.

 Board: ESP32 dev board (WROOM). Tools > Board > "ESP32 Dev Module".

 Libraries (install the exact names below via Library Manager — use the maintained
 ESP32Async forks; the older "ESPAsyncWebServer"/"AsyncTCP" forks fail to build on
 ESP32 core 3.x):
 - "WebSockets"          by Markus Sattler (Links2004/arduinoWebSockets)
 - "ESP Async WebServer" by ESP32Async      (note the spaces)
 - "Async TCP"           by ESP32Async      (note the space)
 - WiFi, LittleFS, FS    (bundled with the ESP32 core)
 Verified: compiles for esp32:esp32:esp32 with ESP Async WebServer 3.11.1 +
 Async TCP 3.4.10 + WebSockets 2.7.2 on esp32 core 3.3.10.

 Wiring:
 - ATmega TX (D1, 5V) --[1k]--+--[2k]--GND, node --> ESP32 RX2 (GPIO16)
   (the divider drops 5V to ~3.3V; ESP RX is NOT 5V tolerant)
 - Common ground between the two boards.
 - Upload the dashboard files to LittleFS (ESP32 LittleFS uploader / arduino-cli).
*/

#include <WiFi.h>
#include <WebSocketsServer.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>

// ---- config ----
const char *AP_SSID = "HondaOBD";
const char *AP_PASS = "honda1234"; // >= 8 chars; change me
const uint8_t UART_RX_PIN = 16;    // ESP32 RX2 <- ATmega TX (through divider)
const uint8_t UART_TX_PIN = 17;    // unused (one-way), reserved
const uint32_t UART_BAUD = 115200;

WebSocketsServer webSocket(81); // ws://<ip>:81
AsyncWebServer httpServer(80);  // http://<ip>/  (serves the PWA)

// line assembly from the UART stream (larger than the ATmega's JSON buffer so a
// full frame is never dropped at the bridge)
char lineBuf[340];
uint16_t lineLen = 0;

void onWsEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t len) {
  // read-only stream: we never act on inbound messages (keeps the ATmega's
  // pin-write API unreachable from the network). Just log connects.
  if (type == WStype_CONNECTED) {
    Serial.printf("ws client %u connected\n", num);
  }
}

void setup() {
  Serial.begin(115200);                                       // USB debug
  Serial2.begin(UART_BAUD, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN); // from ATmega

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP()); // 192.168.4.1

  webSocket.begin();
  webSocket.onEvent(onWsEvent);

  if (LittleFS.begin(true)) {
    // serve the dashboard PWA; index.html for "/"
    httpServer.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
    httpServer.onNotFound([](AsyncWebServerRequest *req) {
      req->send(LittleFS, "/index.html", "text/html");
    });
    httpServer.begin();
    Serial.println("HTTP server serving /dashboard from LittleFS");
  } else {
    Serial.println("LittleFS mount failed - dashboard not served (WS still works)");
  }
}

void loop() {
  webSocket.loop();

  // drain the UART; on each complete '\n'-terminated JSON line, broadcast it
  while (Serial2.available()) {
    char c = Serial2.read();
    if (c == '\n' || c == '\r') {
      if (lineLen > 0) {
        lineBuf[lineLen] = '\0';
        webSocket.broadcastTXT(lineBuf, lineLen);
        lineLen = 0;
      }
    } else if (lineLen < sizeof(lineBuf) - 1) {
      lineBuf[lineLen++] = c;
    } else {
      lineLen = 0; // overflow guard: drop the malformed line
    }
  }
}
