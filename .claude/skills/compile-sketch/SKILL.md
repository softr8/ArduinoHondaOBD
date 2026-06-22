---
name: compile-sketch
description: Compile a Honda OBD sketch with arduino-cli for the Arduino Nano (ATmega328, atmega328old). Reports flash and SRAM usage. Use when asked to compile, build, or verify a sketch.
disable-model-invocation: true
---

# compile-sketch

Compile a sketch in this repo with the correct board target and report memory usage.

## Board target

`arduino:avr:nano:cpu=atmega328old` (matches `.vscode/arduino.json`).

## Sketches

Each lives in its own dir: `hobd_uni2`, `hobd_uni`, `hobd_elm`, `hobd_lcd`, `hobd_u8g`, `test_elm`, `test_hc05`. Default to `hobd_uni2` (the active sketch) unless the user names another.

## Steps

1. Pick the sketch dir from the user's argument; default `hobd_uni2`.
2. Run:
   ```
   arduino-cli compile --fqbn arduino:avr:nano:cpu=atmega328old <sketch-dir>
   ```
3. Report the result. On success, surface the `Sketch uses ... bytes` (flash) and `Global variables use ... bytes` (SRAM) lines — SRAM is the 2KB constraint, call out if it is high (>1500 bytes static).
4. On failure, show the compiler errors with file:line.

If `arduino-cli` is missing, tell the user to install it (`brew install arduino-cli`) and run `arduino-cli core install arduino:avr`.
