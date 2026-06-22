# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Arduino firmware that talks to a Honda ECU over the 3-pin DLC **K-line** (Honda's
pre-2002 serial datalogger protocol), and re-exposes that data two ways:
- as an **ELM327-compatible OBD2 stream over Bluetooth** (HC-05) so the Android
  **Torque** app can read it, and
- on a **16x2 LCD** on the dashboard (paged display + trip computer + DTC reader).

It is a translation/bridge layer: Honda K-line in, ELM327 + LCD out.

## Build / run

There is no test suite. Verification = **compile with `arduino-cli`**. The board and
LCD library differ per sketch (see the conflict note below).

```bash
# active sketch (Nano)
arduino-cli compile --fqbn arduino:avr:nano:cpu=atmega328old hobd_uni2
# older UNO sketch — needs a DIFFERENT lcd lib (see below)
arduino-cli compile --fqbn arduino:avr:uno --library /path/to/NewLiquidCrystal hobd_uni
```

The compile output's `Global variables use N bytes` line is the number that
matters most — see the SRAM constraint below.

`.vscode/arduino.json` targets `hobd_uni2` on a Nano via the VSCode Arduino
extension (uses arduino-cli under the hood). `port` there is a Windows `COM4`.

### Library dependencies (and a conflict that will bite you)

- `SoftwareSerialWithHalfDuplex` (nickstedman, GitHub) — all sketches, drives the
  single-wire K-line.
- **LCD library differs by sketch and the two collide:**
  - `hobd_uni2` uses **johnrickman `LiquidCrystal_I2C`** — 3-arg constructor
    `lcd(0x3f,16,2)`, `lcd.init()`, `lcd.backlight()`.
  - `hobd_uni` uses **fmalpartida `NewLiquidCrystal`** — 8-arg constructor
    `lcd(0x3f,2,1,0,4,5,6,7)`, `lcd.setBacklightPin(...)`, `POSITIVE`.
  - Both ship a header named `LiquidCrystal_I2C.h`, so only one can be installed
    globally at a time. Compiling the other sketch then fails with
    "no matching function for call to 'LiquidCrystal_I2C::...'". To compile a
    sketch whose lib isn't the globally-installed one, force it with
    `arduino-cli compile --library <path-to-correct-lib>`.

## Sketches (which to touch)

- **`hobd_uni2`** — the live one. Arduino **Nano**, I2C LCD, adds NTC thermistor,
  fuel-pressure, and AC-compressor control over `hobd_uni`. Edit this by default.
- **`hobd_uni`** — older **UNO** version, parallel/pot LCD. Frozen but still used
  on UNO hardware. A fork of the same logic — see parity note.
- `hobd_elm`, `hobd_lcd` — deprecated split versions (Bluetooth-only / LCD-only),
  superseded by the unified sketches.
- `hobd_u8g` — experiment: 128x64 SPI graphic LCD via U8glib.
- `test_elm`, `test_hc05` — standalone bench tests (ELM327 simulator, HC-05 setup).

**`hobd_uni` and `hobd_uni2` are forks of the same firmware and drift.** A bug fix
or feature in one usually needs porting to the other; they are not identical
(different board, LCD lib, variable scoping, and display pages), so port
deliberately, don't copy blindly.

## Architecture (the big picture)

Everything runs from `loop()`, which switches between two mutually exclusive modes
based on Bluetooth activity:
- **ELM/Bluetooth mode** (`procbtSerial`) — entered when BT bytes arrive, held for
  2s after the last byte. Parses ELM327 AT commands and OBD2 mode-01/03/04 PIDs,
  answers in ELM hex text. While in this mode the LCD datalog loop is paused.
- **Datalog mode** (`hobd_uni2: execEvery(250)` / `hobd_uni: procdlcSerial`) — runs
  every 250 ms when not in BT mode: reads the ECU, updates globals, computes the
  trip computer, drives alarms/AC, and renders the current LCD page.

Three layers worth understanding before editing:

1. **K-line transport** — `dlcInit()` sends the Honda wake-up sequence;
   `dlcCommand(cmd, num, loc, len)` is the workhorse: it writes a 5-byte request
   (with a Honda checksum = `0xFF - (sum - 1)`), reads `len+3` bytes into the
   global `dlcdata[]`, and validates the response checksum. Live data is read as
   16-byte "rows" at addresses `0x00/0x10/0x20/0x30`; DTCs at `0x40`. Sensor values
   are fixed byte offsets into `dlcdata[]` with per-value scaling formulas
   (see `readEcuData`/`procdlcSerial`).

2. **ELM327 emulation** — `procbtSerial` mimics enough of an ELM327 for Torque:
   standard `AT*` config commands and a subset of OBD2 PIDs, each implemented by
   issuing the matching `dlcCommand` and reformatting Honda data into the OBD2 PID
   response. Note the project's **custom AT extensions**: `21AA`/`22AA`/`24AA` read
   1/2/4 bytes directly from any Honda RAM address (use these to probe/map an
   unknown ECU before hardcoding offsets), and `ATSAP`/`ATDAP`/`ATPAP` read/write
   Arduino pins (keyless-entry / relay control).

3. **Display** — paged 16x2 LCD via `procDisplay`/inline; `lcdZeroPaddedPrint`
   does fixed-width and faux-decimal formatting. A short button press cycles pages,
   a 3s press toggles OBD1/OBD2, a 5s press scans DTCs.

`obd_select` (1=OBD1, 2=OBD2, persisted in EEPROM byte 0) changes the **RPM
formula** and is the main protocol switch. Tested on P2T (OBD2) and P30 (OBD1).

## Critical constraints

- **SRAM is the scarce resource: ATmega328 has 2KB.** Stack/heap collision causes
  no compile error, just random runtime corruption. Keep `Global variables use`
  comfortably under ~1500 bytes. Wrap string literals in `F(...)`, prefer
  `char[]`+`snprintf` over the `String` class, and put lookup tables in `PROGMEM`.
  The `.claude/agents/avr-memory-reviewer` agent checks for these.
- **AVR `int` is 16-bit.** Sensor math overflows easily (e.g. `rpm * map`); cast to
  `long` before multiplying, and watch for integer division producing 0
  (`80/100`). These are recurring bug classes here.
- **K-line address maps are ECU-specific.** Byte offsets and switch-bit positions
  valid for one Honda ECU generation may be wrong on another. Verify with the
  `21AA` probe on the actual car before trusting a new offset.

## Repo tooling

`.claude/` ships a compile-check hook (PostToolUse, runs `arduino-cli compile` on
`.ino` edits), two review agents (AVR memory, OBD protocol), and a
`/compile-sketch` skill. **The hook currently hardcodes the Nano fqbn**, so it
reports false failures when editing the UNO sketch (`hobd_uni`) — that failure is
the board/lib mismatch, not the edit.
