#!/usr/bin/env bash
# One-command ESP32 deploy for the WiFi dashboard.
#
#   tools/deploy-esp.sh [PORT]
#
# Does, in order:
#   1. compile  hobd_esp
#   2. upload   hobd_esp (the firmware)
#   3. build    a LittleFS image from the dashboard's runtime files
#   4. flash    that image to the ESP filesystem partition
#
# PORT is auto-detected if omitted (first USB serial board arduino-cli sees).
# Requires: arduino-cli with the esp32 core + libs installed (see hobd_esp.ino).
#
# Assumes the DEFAULT 4MB partition scheme (esp32:esp32:esp32):
#   LittleFS at offset 0x290000, size 0x160000. Override with FS_OFFSET/FS_SIZE.

set -euo pipefail

REPO="$(cd "$(dirname "$0")/.." && pwd)"
SKETCH="$REPO/hobd_esp"
DASH="$REPO/dashboard"
FQBN="esp32:esp32:esp32"
BAUD="${BAUD:-921600}"
FS_OFFSET="${FS_OFFSET:-0x290000}"
FS_SIZE="${FS_SIZE:-0x160000}"

# runtime files served to the browser (everything else in dashboard/ is dev-only)
ASSETS=(index.html style.css app.js dtc-codes.js manifest.webmanifest sw.js
        icon-192.png icon-512.png)

say() { printf '\033[1;33m==>\033[0m %s\n' "$*"; }
die() { printf '\033[1;31mError:\033[0m %s\n' "$*" >&2; exit 1; }

command -v arduino-cli >/dev/null || die "arduino-cli not found"

# --- resolve port (only real USB serial; skip Bluetooth/audio/debug ttys) ---
PORT="${1:-}"
if [ -z "$PORT" ]; then
  PORT="$(arduino-cli board list 2>/dev/null | awk 'tolower($1) ~ /usb/ && /serial/ {print $1; exit}' || true)"
fi
[ -n "$PORT" ] || die "no USB serial board found; pass one: tools/deploy-esp.sh /dev/cu.usbserial-XXXX"
say "Port: $PORT"

# --- locate esp32 core tools (newest installed) ---
DATA="$(arduino-cli config get directories.data 2>/dev/null)"
[ -n "$DATA" ] || DATA="$HOME/Library/Arduino15"   # fallback (macOS default)
PKG="$DATA/packages/esp32/tools"
MKLITTLEFS="$(find "$PKG/mklittlefs" -name mklittlefs -type f 2>/dev/null | sort | tail -1 || true)"
ESPTOOL="$(find "$PKG/esptool_py" -name esptool -type f 2>/dev/null | sort | tail -1 || true)"
[ -x "$MKLITTLEFS" ] || die "mklittlefs not found (install the esp32 core)"
[ -x "$ESPTOOL" ]    || die "esptool not found (install the esp32 core)"

# --- 1 & 2: compile + upload firmware ---
say "Compiling hobd_esp..."
arduino-cli compile --fqbn "$FQBN" "$SKETCH"
say "Uploading firmware to $PORT ..."
arduino-cli upload -p "$PORT" --fqbn "$FQBN" "$SKETCH"

# --- 3: stage runtime files and build the LittleFS image ---
STAGE="$(mktemp -d)"
trap 'rm -rf "$STAGE"' EXIT
for f in "${ASSETS[@]}"; do
  [ -f "$DASH/$f" ] && cp "$DASH/$f" "$STAGE/" || true
done
[ -f "$STAGE/index.html" ] || die "dashboard/index.html missing"
if [ ! -f "$STAGE/icon-192.png" ]; then
  echo "  note: icon-192.png / icon-512.png not present - PWA install icon will be blank"
fi

IMG="$STAGE/littlefs.bin"
say "Building LittleFS image ($(ls "$STAGE" | grep -vc littlefs) files, size $FS_SIZE)..."
"$MKLITTLEFS" -c "$STAGE" -p 256 -b 4096 -s "$((FS_SIZE))" "$IMG"

# --- 4: flash the filesystem image ---
say "Flashing dashboard to LittleFS at $FS_OFFSET ..."
"$ESPTOOL" --chip esp32 --port "$PORT" --baud "$BAUD" write_flash "$FS_OFFSET" "$IMG"

say "Done. Join WiFi 'HondaOBD' and open http://192.168.4.1"
