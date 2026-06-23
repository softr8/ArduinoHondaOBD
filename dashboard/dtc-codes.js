// Honda MIL code number -> { p: OBD2 P-code(s), d: description }
// Source: the CEL/MIL table in hobd_uni.ino comments (pre-2002 Honda).
// These are MIL *numbers* emitted by the firmware, not OBD2 P-codes; map here.
window.DTC_CODES = {
  1:  { p: "P0131/P0132", d: "Primary HO2S circuit (Sensor 1)" },
  3:  { p: "P0107/P0108", d: "MAP circuit low/high" },
  4:  { p: "P0335/P0336", d: "CKP sensor circuit" },
  5:  { p: "P0106/P1128/P1129", d: "MAP range / lower-higher than expected" },
  6:  { p: "P0117/P0118", d: "ECT circuit low/high" },
  7:  { p: "P0122/P0123/P1121/P1122", d: "TP sensor circuit / range" },
  8:  { p: "P1359/P1361/P1362", d: "CKP/TDC sensor" },
  9:  { p: "P1381/P1382", d: "Cylinder position sensor" },
  10: { p: "P0111/P0112/P0113", d: "IAT sensor circuit" },
  12: { p: "P1491/P1498", d: "EGR valve lift" },
  13: { p: "P1106/P1107/P1108", d: "BARO circuit" },
  14: { p: "P0505/P1508/P1509/P1519", d: "Idle Air Control valve" },
  17: { p: "P0500/P0501", d: "VSS circuit" },
  20: { p: "P1297/P1298", d: "Electrical Load Detector circuit" },
  21: { p: "P1253", d: "VTEC system malfunction" },
  22: { p: "P1257/P1258/P1259", d: "VTEC system malfunction" },
  23: { p: "P0325", d: "Knock sensor circuit" },
  30: { p: "P1655/P1681/P1682", d: "A/T FI signal A" },
  31: { p: "P1686/P1687", d: "A/T FI signal B" },
  34: { p: "P0560", d: "PCM backup voltage circuit low" },
  41: { p: "P0135/P1166/P1167", d: "Primary HO2S heater (Sensor 1)" },
  45: { p: "P0171/P0172", d: "System too lean / too rich" },
  48: { p: "P1162/P1168/P1169", d: "Primary HO2S circuit" },
  54: { p: "P1336/P1337", d: "CSF sensor" },
  58: { p: "P1366/P1367", d: "TDC sensor No.2" },
  61: { p: "P0133/P1149", d: "Primary HO2S slow response (Sensor 1)" },
  63: { p: "P0137/P0138/P0139", d: "Secondary HO2S circuit (Sensor 2)" },
  65: { p: "P0141", d: "Secondary HO2S heater (Sensor 2)" },
  67: { p: "P0420", d: "Catalyst system efficiency low" },
  70: { p: "P0700/P1660", d: "A/T concerns" },
  71: { p: "P0301", d: "Misfire cyl. 1 / random" },
  72: { p: "P0302", d: "Misfire cyl. 2 / random" },
  73: { p: "P0303", d: "Misfire cyl. 3 / random" },
  74: { p: "P0304", d: "Misfire cyl. 4 / random" },
  80: { p: "P0401", d: "EGR insufficient flow" },
  86: { p: "P0116", d: "ECT circuit range/performance" },
  90: { p: "P1456/P1457", d: "EVAP leak detected" },
  91: { p: "P0451/P0452/P0453", d: "Fuel tank pressure sensor" },
  92: { p: "P0441/P1459", d: "EVAP improper purge flow" },
};

window.dtcLabel = function (n) {
  var e = window.DTC_CODES[n];
  return e ? { code: e.p, desc: e.d } : { code: "MIL " + n, desc: "unknown code" };
};
