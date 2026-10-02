# Building from the command line

`fw` (Linux, macOS) and `fw.cmd` (Windows) drive the CMake project in the
repository root. They use the same `build/` directory as the VS Code tasks, so the two
can be mixed freely.

## Prerequisites

| Platform | Install |
| --- | --- |
| Ubuntu 24.04+, Debian 12+ | `sudo apt install build-essential cmake ninja-build git curl python3-venv` |
| macOS | `xcode-select --install`, then `brew install cmake ninja` |
| Windows 10+ | [Git for Windows](https://gitforwindows.org/), [CMake](https://cmake.org/download/), [Ninja](https://ninja-build.org/) and [Python 3](https://www.python.org/downloads/) on `PATH`; or the Raspberry Pi Pico VS Code extension, which installs them all under `%USERPROFILE%\.pico-sdk` |

The scripts check for all of these first and list everything missing in one go, before
anything is downloaded. A host C++ compiler is on the list because the SDK builds its own
`pioasm` and `picotool` for the host when no installed one matches; on Windows the VS Code
extension ships both prebuilt.

Clone with submodules:

```shell
git clone --recursive https://github.com/flipperdevices/flipperone-mcu-firmware.git
```

The Pico SDK and the ARM GCC toolchain are not on the list. The scripts look for them in
`~/.pico-sdk` (`%USERPROFILE%\.pico-sdk` on Windows), the directory the Raspberry Pi Pico
VS Code extension installs into, and fetch whatever is missing into the same place: the
SDK as a git clone, the toolchain as an Arm GNU Toolchain release. The versions are the
ones pinned in `CMakeLists.txt` (`sdkVersion`, `toolchainVersion`), so the extension and
the scripts always agree and either one can install for the other. The first fetch takes
about 1.5 GB of disk: 0.4 GB for the SDK and 1 GB for the unpacked toolchain.

## Commands

| Linux / macOS | Windows | Effect |
| --- | --- | --- |
| `./fw` | `fw.cmd` | Configure on the first run, then build. Output: `build/flipperone-mcu-firmware.uf2` and `build/partition_table.uf2` |
| `./fw clean` | `fw.cmd clean` | Remove the build directory |
| `./fw flash` | `fw.cmd flash` | Build, then write the partition table and the firmware over SWD with openocd |
| `./fw setup` | `fw.cmd setup` | Fetch the SDK and the toolchain without building |

### Choosing the board

The scripts build `f2` unless told otherwise (`DEFAULT_FW_TARGET` in `fw.cfg`; CMake on
its own defaults to `f1`). `FW_TARGET=f1 ./fw` (on Windows `set FW_TARGET=f1`, then
`fw.cmd`) reconfigures the build directory for another board, which means a full
rebuild. The build directory then keeps that board until `FW_TARGET` is set again or
`clean` is run. [TARGETS.md](../TARGETS.md) describes what a target is.

### Flashing

`flash` runs `targets/flash.tcl`, which writes the partition table, the firmware into slot A
and invalidates slot B, exactly like the VS Code "Flash" task. It needs a CMSIS-DAP debug
probe and an openocd that knows the RP2350. The openocd installed by the VS Code extension
is used when present; otherwise `openocd` on `PATH` must be the
[Raspberry Pi build](https://github.com/raspberrypi/openocd). `OPENOCD=/path/to/openocd`
selects one explicitly.

To load over USB instead, put the board into BOOTSEL mode and run
`picotool load -fx build/flipperone-mcu-firmware.uf2`.

## Settings

`fw.cfg` holds the knobs: the default board, where missing pieces are fetched from, and
the openocd configuration files. Environment variables override what the scripts derive:
`PICO_SDK_PATH` and `PICO_TOOLCHAIN_PATH` point at your own SDK or toolchain (nothing is
fetched then), `OPENOCD` at the openocd to use.

With the VS Code extension installed, its hook in `CMakeLists.txt` points the build at
`~/.pico-sdk` regardless of `PICO_SDK_PATH`.

## Troubleshooting

- **`Directory '.../.pico-sdk/sdk/<version>' not found`** in a VS Code build: the
  extension's hook asks for an SDK version that is not installed yet. `./fw setup`
  fetches it into the place the hook looks.
- **Errors naming two different SDK paths** after the SDK moved: a build directory cannot
  be re-pointed at another SDK. Run `./fw clean` and build again.
- **picotool is built from source** when no installed one matches the SDK version. That
  needs a host C++ compiler: `build-essential` on Ubuntu, the Xcode Command Line Tools on
  macOS. On Windows the VS Code extension provides a prebuilt picotool.
