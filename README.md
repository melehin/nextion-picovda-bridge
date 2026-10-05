# nextion-picovda-bridge

Nextion-style UART UI bridge for Raspberry Pi Pico → **VGA** using [PicoVGA](https://github.com/codaris/picovga-cmake) (API only; library is not patched).

Author layouts in a compact `.nxscene` text DSL, compile to a tiny `.nxb` binary, then update texts/values/pages at runtime with Nextion-like commands.

## Features

- Hybrid low-RAM display: `COLOR` strips + `FTEXT` + small `GRAPH8` draw zone (320×240, 1 layer)
- Precompiled scene format (`.nxb`) with host compiler
- Runtime object store + Nextion command subset over UART
- Untouched PicoVGA dependency (git submodule)

## Quick start

### Requirements

- [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk) (`PICO_SDK_PATH`)
- `cmake`, `gcc-arm-none-eabi`, `python3`
- PicoVGA submodule (see below)

### Clone

```bash
git clone --recurse-submodules https://github.com/<you>/nextion-picovda-bridge.git
cd nextion-picovda-bridge
```

If you already cloned without submodules:

```bash
git submodule update --init --recursive
```

### Build firmware

```bash
export PICO_SDK_PATH=/path/to/pico-sdk
mkdir -p build && cd build
cmake .. -DPICO_BOARD=pico
make -j
```

Flash `nextion_picovda_bridge.uf2` (BOOTSEL mode).

Optional: `-DPICOVGA_PATH=/other/picovga-cmake` if the library is not under `third_party/`.

## Project layout

```
include/          Scene binary format, object store, parser, render APIs
src/              Firmware + project-local vga_config.h
scene/            .nxscene layout sources
tools/            nxscene_compile.py (host)
third_party/      PicoVGA submodule (read-only for this project)
generated/        Build artifact (.nxb + C header) — not committed
```

## Create your own layout

1. Copy a scene:

```bash
cp scene/demo_main.nxscene scene/mypanel.nxscene
```

2. Edit as plain text (there is no dedicated `.nxscene` GUI — use any editor).

3. Compile:

```bash
python3 tools/nxscene_compile.py scene/mypanel.nxscene \
  -o generated/mypanel.nxb \
  --header generated/mypanel_nxb.h \
  --symbol mypanel_nxb
```

4. Point firmware at it (`src/main.cpp`):

```cpp
#include "mypanel_nxb.h"
// ...
store_load_nxb(&Store, mypanel_nxb, mypanel_nxb_len);
```

Or change `SCENE_SRC` / symbol names in `CMakeLists.txt`, then rebuild.

### Scene DSL sketch

```text
meta
  size 320 240
  font 1 8x16

page home 0 bg=#001830
  plan
    strip 32
      seg color 320 bg=#102848
    strip 16
      seg ftext 320 font=1 bg=#102848 buf=ftext0
    strip 192
      seg ftext 320 font=1 bg=#001830 buf=ftext1

  text t0 x=8 y=36 font=1 color=#ffffff "Title"
  num  n0 x=8 y=64 font=1 color=#ffff00 val=0 digits=4
```

| Want | Use |
|------|-----|
| Solid header / background | `seg color` |
| Labels / numbers | `seg ftext` + `text` / `num` |
| Circles / free boxes | `seg graph8` + objects tagged `graph` |
| Another screen | new `page` with its own `plan` |

Strip heights should sum to **240**; segment widths in a strip to **320**.

See `scene/demo_main.nxscene` for a full example (color + ftext + graph zone).

## Wiring

See **[docs/wiring.md](docs/wiring.md)** for VGA resistor network + Nextion UART (GP16/GP17).  
Do not use GP0/GP1 for serial — they are VGA blue.

## UART protocol (host → Pico)

Default: **UART0**, **115200 8N1**, TX=**GP16**, RX=**GP17** (change in `src/main.cpp`).

Each command ends with three bytes `0xFF 0xFF 0xFF`.

Supported subset:

| Command | Example |
|---------|---------|
| Change page | `page main` / `page 0` |
| Set text | `t0.txt="Hello"` |
| Set number | `n0.val=42` |
| Visibility | `vis c1,0` |
| Position | `t0.x=8` `t0.y=36` |
| Colors (approx) | `t0.pco=...` `t0.bco=...` |

Object names are resolved on the **current page** (Nextion-style).

## Memory notes

PicoVGA data (fonts, frame buffers) must live in **RAM**. This project avoids a full-screen 8-bit framebuffer:

- `src/vga_config.h` — `LAYERS=1`, `MAXX=320`, `MAXY=240`
- Prefer COLOR/FTEXT; keep GRAPH8 regions small

## License

- This project: [MIT](LICENSE)
- PicoVGA / Pico SDK: see their upstream licenses

## Acknowledgments

- [PicoVGA / picovga-cmake](https://github.com/codaris/picovga-cmake) by Miroslav Nemecek / Wayne Venables
- [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk)
