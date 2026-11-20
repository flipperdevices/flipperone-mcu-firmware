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

They need the pinned packages of `assets/python/requirements.txt`; CI runs them in
`.github/workflows/agent-tools-tests.yml`.

## Agent Tools

To see and control a Flipper One attached to this machine (screen, buttons, consoles, logs, Linux host, flashing over SWD), read [scripts/agent_tools/AGENT_TOOLS.md](scripts/agent_tools/AGENT_TOOLS.md).

## Opening a pull request

Open the PR as a **draft** against `dev`, so that reviewers, Copilot included (it is asked
on `ready_for_review`), come in only after the checks below. CI does not build a branch
until it has a PR, so open it early. Use a branch in this repository if you can push to
it: CI uploads build artifacts only for those, and fork PRs get none. Fill [.github/pull_request_template.md](.github/pull_request_template.md)
(`gh pr create --body` skips it, so copy it in by hand) and keep it short: *Verification*
lists only steps a reviewer can realistically repeat, other cases you tried get two or
three words each. Leave the *Contributor checklist* unticked: it is the human author's
personal attestation, and they tick it.

Then make these checks as separate passes, not along the way while editing:

1. **Ownership.** For every state, resource and decision the PR touches, make sure that:
   - it has exactly one owner, and that owner is the right module;
   - all its code lives in the owner, and other modules go through the owner without
     reading or duplicating its internal state;
   - no case is handled in two places;
   - no case is left uncovered because a check sits in the wrong place.

   If something is in the wrong place, stop. Describe the target structure to the user
   (who owns what, what moves where) and let them decide whether to widen the PR.
2. **Comments.** Every comment the PR adds or touches is needed, current and short. It
   explains what the code cannot say (a non-obvious reason or constraint); it doesn't
   retell the code next to it or repeat an option's own description.
3. **Documentation.** The same for every document the PR adds or touches. User
   documentation follows the user's scenario. The product is in development and its
   procedures, buttons and names still change, so link the single source of truth (the
   page on docs.flipper.net) instead of retelling its steps or UI details.
4. **Hardware.** Run a user-facing flow on hardware the way the user will before claiming
   it works. A procedure quoted in an issue may be stale. Without a device, write "not
   tried on hardware" in *Verification* instead.
5. **Self-review.** Have a fresh subagent on the most capable model available review the
   PR. If you are not sure which model that is, ask the user; either way, tell the user
   which model reviewed. Give it only the PR: no memory, no notes, nothing from your session, so it reads
   the change as the human reviewer will. Fix the serious remarks (bugs, architecture,
   misleading documentation; not style or taste), pass the fixes through checks 2 and 3
   again. If serious remarks were fixed, run one more round with a new subagent. After
   two rounds, hand what is still open to the user.

Only then mark the PR **ready for review**. Before pushing follow-up commits, check that it
is still open: commits pushed after a merge never reach `dev`.

## Rules that are easy to get wrong

- `malloc`/`calloc`/`free`/`strdup` are remapped onto `pvPortMalloc`: allocation never
  returns `NULL` (it crashes), memory comes back zeroed, `malloc(0)` crashes, and
  allocating from an ISR crashes. Never add `NULL` checks.
- No `goto`. Error paths unwind with `do { ... break; } while(false)`.
- Inside `targets/` use `<>` includes only — quoted includes defeat header shadowing.
- Services are permanent threads with no teardown; that is not a leak.
- `*.pb.c/h`, `assets.h`, image `.c` files, `pb2/`, `targets/memory_flash.incl` and
  `targets/flash.tcl` are generated. Edit the generator or the source of truth instead.
