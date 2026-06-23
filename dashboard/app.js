// Honda OBD live cluster — WebSocket client.
// Parses the JSON frames the ATmega328 streams (schema in README.md) and drives
// the tachometer shift-lights, speed/rpm heroes, instrument tiles, and MIL lamp.

const params = new URLSearchParams(location.search);
const WS_URL =
  params.get("ws") ||
  (location.protocol === "file:"
    ? "ws://192.168.4.1:81"
    : `ws://${location.hostname}:81`);

const STALE_MS = 1500;
const TACH_MAX = 8500;   // B16 ~8200 redline + margin
const REDLINE = 7600;
const SEGMENTS = 24;

// instrument tiles (rpm + vss are the heroes, not tiles).
// scale=10 => firmware sent deci-units; max => draws a bar.
const TILES = [
  { key: "ect",  label: "COOLANT", unit: "°C", max: 120, warn: 98, alert: 110 },
  { key: "iat",  label: "INTAKE",  unit: "°C", max: 80 },
  { key: "map",  label: "MAP",     unit: "kPa", max: 105 },
  { key: "tps",  label: "THROTTLE", unit: "%", max: 100 },
  { key: "volt", label: "BATTERY", unit: "V", scale: 10, max: 16, alertLow: 11.5 },
  { key: "ign",  label: "TIMING",  unit: "°", scale: 10 },
  { key: "inj",  label: "INJ",     unit: "ms", max: 16 },
  { key: "iac",  label: "IAC",     unit: "%", max: 100 },
  { key: "sft",  label: "SHORT FT", unit: "%" },
  { key: "lft",  label: "LONG FT",  unit: "%" },
  { key: "knoc", label: "KNOCK",   unit: "", max: 5, warn: 1, alert: 3 },
  { key: "vavg", label: "AVG",     unit: "km/h", max: 200 },
];

// ---- build shift-lights ----
const slEl = document.getElementById("shiftlights");
for (let i = 0; i < SEGMENTS; i++) {
  const f = i / (SEGMENTS - 1);
  const seg = document.createElement("span");
  seg.className = "seg " + (f < 0.55 ? "z-green" : f < 0.82 ? "z-amber" : "z-red");
  seg.style.setProperty("--i", i);
  slEl.appendChild(seg);
}
const segs = [...slEl.children];

// ---- build tiles ----
const gaugesEl = document.getElementById("gauges");
const tiles = {};
TILES.forEach((t, i) => {
  const el = document.createElement("div");
  el.className = "tile";
  el.style.setProperty("--i", i);
  el.innerHTML =
    `<span class="t-label">${t.label}</span>` +
    `<span><span class="t-value" data-v>--</span>` +
    (t.unit ? `<span class="t-unit">${t.unit}</span>` : "") + `</span>` +
    (t.max ? `<span class="t-bar"><i data-bar></i></span>` : "");
  gaugesEl.appendChild(el);
  tiles[t.key] = { el, val: el.querySelector("[data-v]"), bar: el.querySelector("[data-bar]"), def: t };
});

const rpmEl = document.getElementById("rpm");
const vssEl = document.getElementById("vss");
const linkEl = document.getElementById("link");
const linkTxt = linkEl.querySelector(".link-txt");
const updatedEl = document.getElementById("updated");
const errsEl = document.getElementById("errs");
const dtcEl = document.getElementById("dtc");
const dtcList = document.getElementById("dtc-list");
const dtcTitle = document.getElementById("dtc-title");
const dtcCount = document.getElementById("dtc-count");

let lastFrame = 0;

function render(d) {
  lastFrame = Date.now();

  // tachometer
  if (d.rpm !== undefined) {
    rpmEl.textContent = String(d.rpm).padStart(4, "0");
    const lit = Math.round(Math.min(1, d.rpm / TACH_MAX) * SEGMENTS);
    segs.forEach((s, i) => s.classList.toggle("lit", i < lit));
    slEl.classList.toggle("redline", d.rpm >= REDLINE);
  }
  if (d.vss !== undefined) vssEl.textContent = d.vss;

  // tiles
  for (const t of TILES) {
    const tile = tiles[t.key];
    let v = d[t.key];
    if (v === undefined || v === null) continue;
    if (t.scale) v = v / t.scale;
    tile.val.textContent = t.scale ? v.toFixed(1) : v;

    tile.el.classList.remove("warn", "alert");
    if (t.alert !== undefined && v >= t.alert) tile.el.classList.add("alert");
    else if (t.alertLow !== undefined && v > 0 && v < t.alertLow) tile.el.classList.add("alert");
    else if (t.warn !== undefined && v >= t.warn) tile.el.classList.add("warn");

    if (tile.bar && t.max) {
      tile.bar.style.width = Math.max(0, Math.min(100, (v / t.max) * 100)) + "%";
    }
  }

  renderDtc(d);
  updatedEl.textContent = "updated " + new Date(lastFrame).toLocaleTimeString();
  if (d.ec !== undefined || d.et !== undefined)
    errsEl.textContent = `LINK ${d.ec || 0} CHK / ${d.et || 0} TMO`;
}

function renderDtc(d) {
  const codes = Array.isArray(d.dtc) ? d.dtc : [];
  const mil = d.mil || codes.length > 0;
  dtcEl.classList.toggle("bad", !!mil);
  dtcEl.classList.toggle("ok", !mil);
  dtcCount.textContent = codes.length ? codes.length + " CODE" + (codes.length > 1 ? "S" : "") : "";
  if (codes.length === 0) {
    dtcList.innerHTML = '<li class="none">NO STORED CODES</li>';
    return;
  }
  dtcList.innerHTML = "";
  for (const n of codes) {
    const { code, desc } = window.dtcLabel(n);
    const li = document.createElement("li");
    li.innerHTML = `<span class="code">${code}</span><span class="desc">${desc}</span>`;
    dtcList.appendChild(li);
  }
}

function setLink(state) {
  linkEl.className = "link " + state;
  linkTxt.textContent = state === "up" ? "LIVE" : state === "stale" ? "STALE" : "NO LINK";
}

setInterval(() => {
  if (lastFrame && Date.now() - lastFrame > STALE_MS && linkEl.classList.contains("up"))
    setLink("stale");
}, 500);

let ws;
function connect() {
  setLink("down");
  ws = new WebSocket(WS_URL);
  ws.onopen = () => setLink("up");
  ws.onclose = () => { setLink("down"); setTimeout(connect, 1500); };
  ws.onerror = () => ws.close();
  ws.onmessage = (ev) => {
    let d;
    try { d = JSON.parse(ev.data); } catch { return; }
    setLink("up");
    render(d);
  };
}
connect();

if ("serviceWorker" in navigator) navigator.serviceWorker.register("sw.js").catch(() => {});
