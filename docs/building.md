# Building from the command line

`fw` (Linux, macOS) and `fw.cmd` (Windows) drive the CMake project in the
repository root. They use the same `build/` directory as the VS Code tasks, so the two
can be mixed freely.

## Prerequisites

| Platform | Install |
| --- | --- |
| Ubuntu 24.04+, Debian 12+ | `sudo apt install build-essential cmake ninja-build git curl python3-venv` |
| macOS | `xcode-select --install`, then `brew install cmake ninja python` |
| Windows 10+ | [Git for Windows](https://gitforwindows.org/). CMake, Ninja and Python are fetched by `fw.cmd` when neither `PATH` nor the Raspberry Pi Pico VS Code extension provides them |

On Linux and macOS the script checks for all of these first and lists everything missing in
one go, before anything is downloaded. A host C++ compiler is on the list because the SDK
builds its own `pioasm` and `picotool` for the host. On macOS the Python that comes with the
command line tools is not enough: it refuses to create the virtual environment the assets
step uses, hence the Homebrew one.

On Windows only Git has to be installed by hand. `fw.cmd` fetches CMake and Ninja (zip
archives), Python (the python.org installer, per user, with the `py` launcher that
`assets/python/run_venv.cmd` calls) and prebuilt `pioasm` and `picotool` into
`%USERPROFILE%\.pico-sdk`, in the layout the VS Code extension uses. Nothing is added to
`PATH`: the script locates the tools itself, so the same terminal can go on with `fw.cmd build`
right after the fetch.

Clone with submodules:

```shell
git clone --recursive https://github.com/flipperdevices/flipperone-mcu-firmware.git
```

The Pico SDK and the ARM GCC toolchain are not on the list. The scripts look for them in
`~/.pico-sdk` (`%USERPROFILE%\.pico-sdk` on Windows), the directory the Raspberry Pi Pico
VS Code extension installs into, and fetch whatever is missing into the same place: the
SDK as a git clone, the toolchain as an Arm GNU Toolchain release. The versions are the
ones pinned in `CMakeLists.txt` (`sdkVersion`, `toolchainVersion`, `picotoolVersion`), so the
extension and the scripts always agree and either one can install for the other. The first
fetch takes about 1.5 GB of disk: 0.4 GB for the SDK and 1 GB for the unpacked toolchain,
plus some 0.2 GB for the host tools on Windows.

## Commands

| Linux / macOS | Windows | Effect |
| --- | --- | --- |
| `./fw` | `fw.cmd` | Configure on the first run, then build. Output: `build/flipperone-mcu-firmware.uf2` and `build/partition_table.uf2` |
| `./fw clean` | `fw.cmd clean` | Remove the build directory |
| `./fw flash` | `fw.cmd flash` | Build, then write the partition table and the firmware over SWD with openocd |
| `./fw setup` | `fw.cmd setup` | Fetch everything the build needs without building |

### Choosing the board

The scripts build `f2` unless told otherwise (`DEFAULT_FW_TARGET` in `fw.cfg`; CMake on
its own defaults to `f1`). `./fw build FW_TARGET=f1` (Windows: `fw.cmd build FW_TARGET=f1`)
reconfigures the build directory for another board, which means a full rebuild. The build
directory then keeps that board until `FW_TARGET` is given again or `clean` is run. [TARGETS.md](../TARGETS.md) describes what a target is.

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

`fw.cfg` holds the knobs: the default board, where missing pieces are fetched from, the
openocd configuration files, and for Windows the CMake, Ninja and Python versions plus the
`pico-sdk-tools` release that provides the prebuilt `pioasm` and `picotool`. Any variable can be overridden for one run make-style,
`./fw build NAME=value`, or through the environment: `PICO_SDK_PATH` and
`PICO_TOOLCHAIN_PATH` point at your own SDK or toolchain (nothing is fetched then),
`OPENOCD` at the openocd to use. On Windows a value with spaces has to be set with `set`
before running `fw.cmd`, since the command line is split on spaces.

With the VS Code extension installed, its hook in `CMakeLists.txt` points the build at
`~/.pico-sdk` regardless of `PICO_SDK_PATH`.

## Troubleshooting

- **`Directory '.../.pico-sdk/sdk/<version>' not found`** in a VS Code build: the
  extension's hook asks for an SDK version that is not installed yet. `./fw setup`
  fetches it into the place the hook looks.
- **Errors naming two different SDK paths** after the SDK moved: a build directory cannot
  be re-pointed at another SDK. Run `./fw clean` and build again.
- **`This build of python cannot create venvs without using symlinks`** on macOS: `python3`
  is the Apple command line tools build. `brew install python` and make sure Homebrew's
  `python3` comes first on `PATH`.
- **picotool is built from source** on Linux and macOS when no installed one matches the
  SDK version. That needs a host C++ compiler: `build-essential` on Ubuntu, the Xcode
  Command Line Tools on macOS. Windows gets a prebuilt one from `fw.cmd`.
