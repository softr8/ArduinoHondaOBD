"""Single source of truth for the hobd_uni protection / interface schematic.

Both outputs are generated from this file by gen.py:
  - the Mermaid diagram in README.md  (fully generated: nodes + edges)
  - images/hobd_uni_schematic.svg     (schemdraw; values from PARTS, layout curated)

Edit values in PARTS and they update in BOTH outputs. Adding/removing a
component: edit NODES/EDGES (Mermaid updates automatically) and the curated
schemdraw layout in gen.py (schematic layout cannot be auto-generated).
"""

# --- component values / part numbers (single-sourced) ---
PARTS = {
    "fuse":         "2A",
    "diode":        "SS54",
    "tvs":          "SMBJ24A",
    "bulk":         "100µF + 100nF",
    "kline_series": "510Ω",
    "kline_xcvr":   "L9637D",
    "lvl_top":      "1kΩ",
    "lvl_bot":      "2kΩ",
    "div_top":      "33kΩ",
    "div_bot":      "10kΩ",
    "div_cap":      "100nF",
    "i2c_pull":     "4.7kΩ",
}

# --- nodes: id -> (label, role) ; role drives Mermaid colour class ---
# use \n for a line break (rendered as <br/> in Mermaid).
NODES = {
    "V12":  ("DLC +12V", "pwr"),
    "DI":   (f"{PARTS['diode']} diode\n(reverse-polarity)", "prot"),
    "TV":   (f"{PARTS['tvs']} TVS\n+ {PARTS['bulk']}", "prot"),
    "VIN":  ("UNO Vin", "mcu"),
    "KL":   ("DLC K-line ~12V", "sig"),
    "LX":   (f"{PARTS['kline_xcvr']}\nK-line transceiver", "prot"),
    "D12":  ("UNO D12", "mcu"),
    "D1":   ("UNO D1 TX · 5V", "mcu"),
    "ER":   ("ESP32 RX2", "mcu"),
    "ESP":  ("ESP32\nWiFi AP + WebSocket :81", "mcu"),
    "PH":   ("Phone / PWA", "ext"),
    "D11":  ("UNO D11 TX · 5V", "mcu"),
    "HRX":  ("HC-05 RX", "mcu"),
    "HTX":  ("HC-05 TX · 3.3V", "mcu"),
    "D10":  ("UNO D10", "mcu"),
    "VB":   ("Battery +12V", "pwr"),
    "A0":   ("UNO A0", "mcu"),
    "AFR":  ("AEM AFR · 0-5V", "sig"),
    "A1":   ("UNO A1", "mcu"),
    "FP":   ("Fuel press · 0.5-4.5V", "sig"),
    "A2":   ("UNO A2", "mcu"),
    "A4":   ("UNO A4 SDA", "mcu"),
    "A5":   ("UNO A5 SCL", "mcu"),
    "LCD":  ("I2C LCD 16x2", "ext"),
}

# --- edges: (src, dst, label, style) ; style in solid|dashed|bidir ---
LVL = f"{PARTS['lvl_top']}/{PARTS['lvl_bot']} to 3.3V"
EDGES = [
    ("V12", "DI",  f"fuse {PARTS['fuse']}", "solid"),
    ("DI",  "TV",  "", "solid"),
    ("TV",  "VIN", "+12V protected", "solid"),
    ("KL",  "LX",  PARTS["kline_series"], "solid"),
    ("LX",  "D12", "single-wire UART", "bidir"),
    ("D1",  "ER",  LVL, "solid"),
    ("ER",  "ESP", "", "solid"),
    ("ESP", "PH",  "ws://192.168.4.1", "dashed"),
    ("D11", "HRX", LVL, "solid"),
    ("HTX", "D10", "", "solid"),
    ("VB",  "A0",  f"{PARTS['div_top']}/{PARTS['div_bot']} + {PARTS['div_cap']}", "solid"),
    ("AFR", "A1",  "", "solid"),
    ("FP",  "A2",  "", "solid"),
    ("A4",  "LCD", f"{PARTS['i2c_pull']} pull-up", "solid"),
    ("A5",  "LCD", f"{PARTS['i2c_pull']} pull-up", "solid"),
]

# Mermaid colour classes per role
ROLE_STYLE = {
    "pwr":  "fill:#3a1010,stroke:#ff5555,color:#fff",
    "sig":  "fill:#10243a,stroke:#55aaff,color:#fff",
    "prot": "fill:#3a2a10,stroke:#ffaa33,color:#fff",
    "mcu":  "fill:#14241a,stroke:#55cc88,color:#fff",
    "ext":  "fill:#241024,stroke:#cc66cc,color:#fff",
}
