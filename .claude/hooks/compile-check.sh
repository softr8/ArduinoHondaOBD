#!/usr/bin/env bash
# PostToolUse hook: compile the edited sketch with arduino-cli.
#
# Board is taken per-sketch from each sketch's sketch.yaml (default_fqbn), so a
# UNO sketch compiles as UNO, the Nano sketch as Nano, and the ESP sketch as
# ESP32 -- no single hardcoded board. Some sketches also need a specific library
# that conflicts with another sketch's library (same header name), so those get
# a per-sketch --library override below.
#
# On success: prints flash/SRAM usage. On failure: prints just the error lines
# (concise) and exits 2 so the error is surfaced.

input=$(cat)
file=$(printf '%s' "$input" | python3 -c 'import sys,json;print(json.load(sys.stdin).get("tool_input",{}).get("file_path",""))' 2>/dev/null)
# fallback if python3 is unavailable, so the hook never silently no-ops
if [ -z "$file" ]; then
  file=$(printf '%s' "$input" | sed -n 's/.*"file_path"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' | head -1)
fi

case "$file" in
  *.ino) ;;
  *) exit 0 ;;
esac

command -v arduino-cli >/dev/null 2>&1 || { echo "arduino-cli not found; skipping compile-check" >&2; exit 0; }

sketch_dir=$(dirname "$file")
sketch_name=$(basename "$sketch_dir")

# Per-sketch library overrides for libs that can't be installed globally at the
# same time (conflicting header names). --library takes precedence over the
# globally-installed lib, so both AVR LCD sketches can build in one environment.
extra=()
case "$sketch_name" in
  hobd_uni)
    # needs fmalpartida NewLiquidCrystal (hobd_uni2 uses johnrickman LiquidCrystal_I2C)
    [ -d "$HOME/Arduino-extra-libs/NewLiquidCrystal" ] && extra=(--library "$HOME/Arduino-extra-libs/NewLiquidCrystal")
    ;;
esac

# Board comes from sketch.yaml (default_fqbn). Fall back to AVR Nano if absent.
fqbn=()
[ -f "$sketch_dir/sketch.yaml" ] || fqbn=(--fqbn arduino:avr:nano:cpu=atmega328old)

out=$(arduino-cli compile "${fqbn[@]}" "${extra[@]}" "$sketch_dir" 2>&1)
status=$?

if [ $status -eq 0 ]; then
  echo "$out" | grep -Ei 'bytes|global variables' >&2
  exit 0
fi

# Failure: surface only the concise error lines, not the whole build log.
echo "Compile FAILED for $sketch_name:" >&2
echo "$out" | grep -E 'error:|fatal error:' | head -8 >&2
exit 2
