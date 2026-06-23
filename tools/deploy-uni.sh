#!/usr/bin/env bash
# One-command setup + build + flash for the hobd_uni (Arduino UNO) sketch.
#
#   tools/deploy-uni.sh [PORT]
#
# Does, in order:
#   1. install deps  - arduino:avr core + the two git-only libraries
#   2. compile       - arduino:avr:uno
#   3. upload        - flash the UNO  (skipped if no board is connected)
#
# PORT is auto-detected if omitted. Steps 1-2 work without a board, so this
# doubles as a setup/verify command.
#
# Why NewLiquidCrystal is kept out of the global libraries dir: it ships a
# LiquidCrystal_I2C.h that collides with the johnrickman lib hobd_uni2 uses, so
# it lives in its own dir and is passed via --library (which takes precedence).

set -euo pipefail

REPO="$(cd "$(dirname "$0")/.." && pwd)"
SKETCH="$REPO/hobd_uni"
FQBN="arduino:avr:uno"
EXTRA_LIB="$HOME/Arduino-extra-libs/NewLiquidCrystal"   # fmalpartida NewLiquidCrystal
SSHD_URL="https://github.com/nickstedman/SoftwareSerialWithHalfDuplex.git"
NLC_URL="https://github.com/fmalpartida/New-LiquidCrystal.git"

say() { printf '\033[1;33m==>\033[0m %s\n' "$*"; }
die() { printf '\033[1;31mError:\033[0m %s\n' "$*" >&2; exit 1; }

command -v arduino-cli >/dev/null || die "arduino-cli not found"
command -v git >/dev/null || die "git not found"

# --- 1: dependencies ---
say "Ensuring arduino:avr core..."
arduino-cli core list 2>/dev/null | grep -q '^arduino:avr' || arduino-cli core install arduino:avr

USERDIR="$(arduino-cli config get directories.user 2>/dev/null)"
[ -n "$USERDIR" ] || USERDIR="$HOME/Documents/Arduino"   # arduino-cli default sketchbook
LIBDIR="$USERDIR/libraries"
mkdir -p "$LIBDIR" "$(dirname "$EXTRA_LIB")"

if [ ! -d "$LIBDIR/SoftwareSerialWithHalfDuplex" ]; then
  say "Cloning SoftwareSerialWithHalfDuplex..."
  git clone --depth 1 "$SSHD_URL" "$LIBDIR/SoftwareSerialWithHalfDuplex"
else
  echo "  SoftwareSerialWithHalfDuplex present"
fi

if [ ! -d "$EXTRA_LIB" ]; then
  say "Cloning NewLiquidCrystal (isolated)..."
  git clone --depth 1 "$NLC_URL" "$EXTRA_LIB"
else
  echo "  NewLiquidCrystal present"
fi

# --- resolve port (only real USB serial; skip Bluetooth/audio/debug ttys) ---
PORT="${1:-}"
if [ -z "$PORT" ]; then
  PORT="$(arduino-cli board list 2>/dev/null | awk 'tolower($1) ~ /usb/ && /serial/ {print $1; exit}' || true)"
fi

# --- 2 & 3: compile (+ upload if a board is present) ---
if [ -n "$PORT" ]; then
  say "Compiling + uploading to $PORT ..."
  arduino-cli compile -u -p "$PORT" --fqbn "$FQBN" --library "$EXTRA_LIB" "$SKETCH"
  say "Done. hobd_uni flashed to $PORT."
else
  say "No USB board detected - compiling only (skipping upload)."
  arduino-cli compile --fqbn "$FQBN" --library "$EXTRA_LIB" "$SKETCH"
  say "Compile OK. Connect the UNO and re-run, or pass the port explicitly."
fi
