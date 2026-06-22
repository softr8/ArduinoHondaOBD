#!/usr/bin/env bash
# PostToolUse hook: compile the edited sketch with arduino-cli.
# Surfaces compile errors AND the SRAM/flash usage line back to Claude.
# Only acts on .ino files; no-op for anything else.

input=$(cat)
file=$(printf '%s' "$input" | python3 -c 'import sys,json; print(json.load(sys.stdin).get("tool_input",{}).get("file_path",""))' 2>/dev/null)

case "$file" in
  *.ino) ;;
  *) exit 0 ;;
esac

command -v arduino-cli >/dev/null 2>&1 || { echo "arduino-cli not found; skipping compile-check" >&2; exit 0; }

sketch_dir=$(dirname "$file")
out=$(arduino-cli compile --fqbn arduino:avr:nano:cpu=atmega328old "$sketch_dir" 2>&1)
status=$?

if [ $status -ne 0 ]; then
  echo "Compile FAILED for $sketch_dir:" >&2
  echo "$out" >&2
  exit 2   # exit 2 = feed stderr back to Claude as actionable error
fi

# Success: report memory usage so SRAM pressure stays visible.
echo "$out" | grep -Ei 'bytes|memory|global variables' >&2
exit 0
