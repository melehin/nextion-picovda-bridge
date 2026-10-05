# Third-party dependencies

## PicoVGA (`picovga-cmake`)

Git submodule: [codaris/picovga-cmake](https://github.com/codaris/picovga-cmake)

```bash
git submodule update --init --recursive
```

Or clone manually:

```bash
git clone https://github.com/codaris/picovga-cmake.git picovga-cmake
```

**Do not edit** files under `picovga-cmake/`. This bridge uses PicoVGA only through its public API. Project-local VGA RAM settings live in `../src/vga_config.h`.
