Honda OBD Live Dashboard (PWA)
==============================

A small, dependency-free web app that shows live Honda OBD data over a WebSocket.
Served by the ESP WiFi co-processor (`hobd_esp`) and installable as a PWA on any
phone/desktop. No build step — plain HTML/CSS/JS.

Data flow
---------
    ATmega328 (hobd_uni) --UART JSON--> ESP (hobd_esp) --WebSocket :81--> this app

The ATmega streams one JSON line every ~250 ms. The ESP rebroadcasts each line to
all connected WebSocket clients.

JSON schema (one object per line)
---------------------------------
All values are **integers**. `volt` and `ign` are sent as **deci-units** (x10) — the
app divides by 10 — because AVR `printf` has no `%f`.

| key   | meaning                         | notes                          |
|-------|---------------------------------|--------------------------------|
| rpm   | engine RPM                      |                                |
| vss   | vehicle speed (km/h)            |                                |
| ect   | coolant temp (°C)               |                                |
| iat   | intake air temp (°C)            |                                |
| map   | manifold pressure (kPa)         |                                |
| tps   | throttle position (%)           |                                |
| volt  | battery voltage x10             | 142 = 14.2 V                   |
| sft   | short fuel trim (%)             |                                |
| lft   | long fuel trim (%)              |                                |
| inj   | injector pulse (ms)             |                                |
| ign   | ignition timing x10 (°)         | 165 = 16.5°                    |
| iac   | idle air control (%)            |                                |
| knoc  | knock (0–5)                     |                                |
| vavg  | average speed (km/h)            |                                |
| vtop  | top speed (km/h)                |                                |
| et    | K-line timeout error counter    | link health, not a DTC         |
| ec    | K-line checksum error counter   | link health, not a DTC         |
| mil   | check-engine lamp (0/1)         |                                |
| dtc   | array of Honda MIL code numbers | `[]` when none; mapped in app  |

`dtc` carries raw Honda MIL **numbers** (e.g. 6, 7, 43). `dtc-codes.js` maps them to
OBD2 P-codes + descriptions for display.

Develop without hardware
------------------------
    npm install ws
    node mock-server.js
    # then open index.html in a browser with:  ?ws=ws://localhost:81
    # e.g. via any static server, or open the file directly

The mock streams realistic moving values and toggles a fault every ~15 s so the
Check-Engine panel can be tested. Kill/restart it to verify auto-reconnect.

Deploy to the ESP
-----------------
One command compiles + uploads the firmware AND builds + flashes the dashboard
to LittleFS:

    tools/deploy-esp.sh [PORT]

(PORT auto-detected if omitted.) Then browse to `http://192.168.4.1` and "Add to
Home Screen". The script flashes only the runtime files (index.html, style.css,
app.js, dtc-codes.js, manifest.webmanifest, sw.js, and icon-192/512.png if
present) — not this README, the mock server, or node_modules.

Assumes the default 4MB partition scheme (LittleFS at 0x290000); override with
`FS_OFFSET=… FS_SIZE=… tools/deploy-esp.sh` for other schemes. Requires the esp32
core + libs from `hobd_esp.ino`.

(Icons are not committed — drop any 192px and 512px PNG into dashboard/.)
