# AGENTS.md

Orientation for AI agents working in this repository. Keep it short and factual —
`docs/TARGETS.md`, `CONTRIBUTING.md` and `.github/copilot-instructions.md` own the details.

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
| `lib/` | First-party `toolbox`, `cli`, `containers`, `drivers`, `uart_pio`, `tusb`; submodules `corelibs`, `freertos`, `micro-ecc`, `nanopb`, `pico-kvstore`, `rtt` |
| `targets/` | Board descriptors (`f1` base, `f2` derived) and generated flash layout |
| `assets/` | PNG/UI sources, protobuf (`assets/proto` submodule), generation scripts |
| `scripts/` | `toolchain/` (toolchain bootstrap and pin), `get_env.py` (CI naming), `testops.py` (on-device test runner), `flash.py` (SWD flashing), `partition_table_update.py` |
| `toolchain.json` | The pinned flipperone-mcu-toolchain release, written by `scripts/toolchain/toolchain.py` |

## Build

```shell
source scripts/toolchain/fbtenv.sh
cmake --preset f2-release
cmake --build --preset f2-release
```

`fbtenv.sh` (`fbtenv.ps1` on Windows) sets up the flipperone-mcu-toolchain release
pinned in `toolchain.json`, downloading it the first time; source it in every new shell.
`FW_TARGET` defaults to `f2`; `f1` is the other board. Presets are in
`CMakePresets.json`, each with its own `build/<preset>/`. Output is
`build/f2-release/flipperone-mcu-firmware.uf2`.

Configure checks the tools against `toolchain.json` (`cmake/toolchain_check.cmake`). Do
not edit `toolchain.json` or the `sdkVersion`/`toolchainVersion`/`picotoolVersion` lines
of `CMakeLists.txt` by hand: `scripts/toolchain/toolchain.py` writes them.

## Test

Firmware unit tests run on a real device over its UART:

```shell
python3 scripts/testops.py sanity --commit <sha> --target f2
python3 scripts/testops.py unit-tests
```

Both require a leased stand (`FO_LEASE` / `/run/fo/lease.json`), which exists only on the
self-hosted `fone-lab` runners. Which board CI covers is defined in the workflows — do not
assume every target has hardware coverage.

The agent tools have host unit tests, with no device and no firmware build:

```shell
python3 -m pytest scripts/agent_tools/tests -q
```

They need the packages of `scripts/requirements-test.txt`; CI runs them in
`.github/workflows/agent-tools-tests.yml`. The tests of `scripts/toolchain` need only
pytest: `python3 -m pytest scripts/toolchain/tests -q`.

## Agent Tools

To see and control a Flipper One attached to this machine (screen, buttons, consoles, logs, Linux host, flashing over SWD), read [scripts/agent_tools/AGENT_TOOLS.md](scripts/agent_tools/AGENT_TOOLS.md).

## Opening a pull request

Follow the pull request steps in [CONTRIBUTING.md](CONTRIBUTING.md#pull-request). On top
of them:

- If you can push to this repository, use a branch in it instead of a fork.
- `gh pr create --body` skips [the PR template](.github/pull_request_template.md), so copy
  it in by hand. Leave the *Contributor checklist* unticked: the human author ticks it.
- Ownership check: the code owner to agree with is the user.
- Self-review: use a fresh subagent on the most capable model available. Give it the PR
  number and ask it to review the PR and report back to you without posting anything on
  GitHub. Add nothing from your session: no memory, no notes (what the harness loads into
  every agent by itself is fine). If you are not sure which
  model is the most capable, ask the user; either way, tell the user which model
  reviewed. If serious remarks were fixed, run one more round with a new subagent. Fix
  the serious remarks of the second round too, but run no third round.
- Never mark the PR ready yourself: when the checks are done, tell the user what is
  still open and leave marking it ready to them.

## Rules that are easy to get wrong

- `malloc`/`calloc`/`free`/`strdup` are remapped onto `pvPortMalloc`: allocation never
  returns `NULL` (it crashes), memory comes back zeroed, `malloc(0)` crashes, and
  allocating from an ISR crashes. Never add `NULL` checks.
- No `goto`. Error paths unwind with `do { ... break; } while(false)`.
- Inside `targets/` use `<>` includes only — quoted includes defeat header shadowing.
- Services are permanent threads with no teardown; that is not a leak.
- `*.pb.c/h`, `assets.h`, image `.c` files, `pb2/` and `targets/memory_flash.incl`
  are generated. Edit the generator or the source of truth instead.
