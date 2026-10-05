# Wiring

Connection scheme for **nextion-picovda-bridge**: PicoVGA → VGA monitor, plus UART for Nextion-style commands.

Do **not** use GP0/GP1 for serial — those pins carry VGA blue.

## Overview

```
Host / Nextion UART ──> Pico GP16 (TX) / GP17 (RX)
Pico GP0..GP8       ──resistors──> VGA connector
Pico USB            ──> power / flash / optional USB console
```

## VGA (PicoVGA)

Same pinout as [PicoVGA Connections](https://codaris.github.io/picovga-cmake/connections.html).

| Pico | Signal | Resistor | VGA |
|------|--------|----------|-----|
| GP0 | B0 (blue LSB) | 1kΩ | Blue |
| GP1 | B1 (blue MSB) | 390Ω | Blue |
| GP2 | G0 | 2.2kΩ | Green |
| GP3 | G1 | 1kΩ | Green |
| GP4 | G2 | 470Ω | Green |
| GP5 | R0 | 2.2kΩ | Red |
| GP6 | R1 | 1kΩ | Red |
| GP7 | R2 | 470Ω | Red |
| GP8 | CSYNC / HSYNC | 100Ω | HSYNC (or CSYNC) |
| GND | ground | — | GND |

### Optional VSYNC

If the monitor needs separate VSYNC, enable in `CMakeLists.txt`:

```cmake
target_compile_definitions(nextion_picovda_bridge PRIVATE VGA_GPIO_VSYNC=9)
```

| Pico | Signal | Resistor | VGA |
|------|--------|----------|-----|
| GP9 | VSYNC | 100Ω | VSYNC |

Default firmware uses mixed sync on GP8 only (typical for many PC monitors).

### Optional audio (unused by this bridge)

| Pico | Signal | Notes |
|------|--------|-------|
| GP19 | PWM audio | PicoVGA default; not required for this project |

## Nextion / host UART

Configured in `src/main.cpp`:

| Setting | Default |
|---------|---------|
| UART | `uart0` |
| Baud | 115200 8N1 |
| Pico TX | **GP16** → device RX |
| Pico RX | **GP17** ← device TX |

| Pico | Direction | Nextion / host |
|------|-----------|----------------|
| GP16 | TX | RX |
| GP17 | RX | TX |
| GND | — | GND |
| 5V or 3.3V | power | per display (many Nextion modules need **5V**) |

Cross TX/RX. Share ground. Level: Pico I/O is **3.3V**; most Nextion boards tolerate 3.3V UART when powered at 5V — check your module datasheet.

Commands end with three bytes `0xFF 0xFF 0xFF`.

## Pin conflict summary

| Pins | Used by |
|------|---------|
| GP0–GP8 | VGA (required) |
| GP9 | VGA VSYNC (optional) |
| GP16–GP17 | Nextion UART (this firmware) |
| GP19 | PWM audio (optional / unused here) |

Free GPIOs (e.g. GP10–GP15, GP18, GP20–GP22) can be remapped for UART by editing `NEXTION_TX_PIN` / `NEXTION_RX_PIN` in `src/main.cpp`.

## Power

- Flash and run from Pico USB, or power via VSYS with a proper supply.
- If the Nextion is 5V-powered, do not feed 5V into Pico GPIO pins; only share GND and use 3.3V UART levels (or a level shifter if required).
