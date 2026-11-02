# AGENTS.md

Orientation for AI agents working in this repository. Keep it short and factual —
`TARGETS.md`, `CONTRIBUTING.md` and `.github/copilot-instructions.md` own the details.

## What this is

Firmware for the Flipper One low-power MCU (Raspberry Pi RP2350B): display, buttons,
touchpad, LEDs, battery and power control. It talks to the Linux CPU over SPI, I²C and
UART. Written in C11 against the Pico SDK and FreeRTOS, with TinyUSB, built by CMake with
the Arm bare-metal GCC toolchain.

Do not quote pinned versions here. The SDK, toolchain and picotool pins live in the build
files and in the build workflow; read them there when a version matters for a review.

## Layout

| Path | Contents |
| --- | --- |
| `CMakeLists.txt` | Board selection, compile flags, flash layout, linking |
| `applications/` | `applications.c` registry of services/apps/CLI commands |
| `applications/services/` | Long-running services, one thread + one event loop each |
| `applications/main/` | Boot-time apps and CLI commands (`cpu`, `dmesg`, `saradc`, `self_check`) |
| `applications/debug/` | Hardware test apps and `unit_test_app` |
| `applications/template/app.c` | Canonical minimal app (`app_alloc` / `app_body` / `app_free`) |
| `lib/` | First-party `toolbox`, `cli`, `containers`, `drivers`, `uart_pio`, `tusb`; submodules `corelibs`, `freertos`, `nanopb`, `pico-kvstore`, `rtt` |
| `targets/` | Board descriptors (`f1` base, `f2` derived) and generated flash layout |
| `assets/` | PNG/UI sources, protobuf (`assets/proto` submodule), generation scripts |
| `scripts/` | `get_env.py` (CI naming), `testops.py` (on-device test runner), `partition_table_update.py` |

## Build

```shell
mkdir -p build && cd build
PICO_SDK_PATH=../../pico-sdk cmake .. -DCMAKE_BUILD_TYPE=Release -DFW_TARGET=f2
cmake --build . --config Release --parallel
```

`FW_TARGET` defaults to `f1`; `f2` is the other board. One build directory, so switching
targets forces a full rebuild. Output is `build/flipperone-mcu-firmware.uf2`.

## Test

There is no host test suite. Unit tests run on a real device over its UART:

```shell
python3 scripts/testops.py sanity --commit <sha> --target f2
python3 scripts/testops.py unit-tests
```

Both require a leased stand (`FO_LEASE` / `/run/fo/lease.json`), which exists only on the
self-hosted `fone-lab` runners. CI does this for `f2` only; `f1` has no hardware coverage.

## Rules that are easy to get wrong

- `malloc`/`calloc`/`free`/`strdup` are remapped onto `pvPortMalloc`: allocation never
  returns `NULL` (it crashes), memory comes back zeroed, `malloc(0)` crashes, and
  allocating from an ISR crashes. Never add `NULL` checks.
- No `goto`. Error paths unwind with `do { ... break; } while(false)`.
- Inside `targets/` use `<>` includes only — quoted includes defeat header shadowing.
- Services are permanent threads with no teardown; that is not a leak.
- `*.pb.c/h`, `assets.h`, image `.c` files, `pb2/`, `targets/memory_flash.incl` and
  `targets/flash.tcl` are generated. Edit the generator or the source of truth instead.
