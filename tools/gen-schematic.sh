#!/usr/bin/env bash
# Regenerate the schematic outputs from the single source of truth
# (tools/schematic/wiring.py):
#   - the Mermaid diagram in README.md (between the SCHEMATIC:MERMAID markers)
#   - images/hobd_uni_schematic.svg (via schemdraw, in a local venv)
#
#   tools/gen-schematic.sh
#
# Edit values in tools/schematic/wiring.py, then run this to update both.

set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
VENV="$HERE/schematic/.venv"
PY="$VENV/bin/python"

if [ ! -x "$PY" ]; then
  echo "==> Creating venv + installing schemdraw (one-time)..."
  python3 -m venv "$VENV"
  "$VENV/bin/pip" install --quiet --upgrade pip
  "$VENV/bin/pip" install --quiet schemdraw
fi

"$PY" "$HERE/schematic/gen.py"
echo "==> Done."
