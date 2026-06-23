// Mock WebSocket server: replays synthetic Honda OBD JSON frames so the dashboard
// can be developed without the car or any hardware.
//
//   npm install ws        (one-time)
//   node mock-server.js   then open index.html?ws=ws://localhost:81
//
// Matches the firmware schema in README.md (integers; volt/ign are deci-units).

const { WebSocketServer } = require("ws");

const PORT = process.env.PORT || 81;
const wss = new WebSocketServer({ port: PORT });
console.log(`mock OBD WS server on ws://localhost:${PORT}`);

let t = 0;
let ec = 0, et = 0;

// flip a fault on/off every ~15s to exercise the DTC panel
function dtcSet() {
  return Math.floor(t / 60) % 2 === 0 ? [] : [6, 7, 43];
}

function frame() {
  t++;
  const rpm = Math.round(820 + 2600 * (0.5 + 0.5 * Math.sin(t / 12)));
  const vss = Math.max(0, Math.round(40 + 40 * Math.sin(t / 20)));
  const dtc = dtcSet();
  if (Math.random() < 0.05) ec++;
  if (Math.random() < 0.03) et++;
  return JSON.stringify({
    rpm,
    vss,
    ect: Math.round(70 + 25 * (0.5 + 0.5 * Math.sin(t / 40))),
    iat: 31,
    map: Math.round(25 + 70 * (rpm / 5000)),
    tps: Math.round(Math.min(100, (rpm - 800) / 60)),
    volt: Math.round((13.8 + 0.4 * Math.sin(t / 8)) * 10),
    sft: Math.round(4 * Math.sin(t / 7)),
    lft: 2,
    inj: Math.round(2 + rpm / 2000),
    ign: Math.round((16 + 8 * Math.sin(t / 10)) * 10),
    iac: Math.round(30 + 10 * Math.sin(t / 15)),
    knoc: dtc.length ? 1 : 0,
    vavg: 38,
    vtop: 92,
    et,
    ec,
    mil: dtc.length ? 1 : 0,
    dtc,
  });
}

wss.on("connection", (ws) => {
  console.log("client connected");
  ws.on("close", () => console.log("client disconnected"));
});

setInterval(() => {
  const msg = frame();
  for (const c of wss.clients) if (c.readyState === 1) c.send(msg);
}, 250);
