# Building

## Quick start

```shell
git clone --recursive https://github.com/flipperdevices/flipperone-mcu-firmware.git
cd flipperone-mcu-firmware

source scripts/toolchain/fbtenv.sh     # PowerShell: . .\scripts\toolchain\fbtenv.ps1
cmake --preset f2-release
cmake --build --preset f2-release
```

The firmware is `build/f2-release/flipperone-mcu-firmware.uf2`. To put it on a device, see
[Flashing.md](Flashing.md).

## Toolchain

Every build tool comes from one pinned release of
[flipperone-mcu-toolchain](https://github.com/flipperdevices/flipperone-mcu-toolchain): Arm
GCC, the Pico SDK, picotool, OpenOCD, CMake, Ninja, clangd and clang-format, and Python
with the packages the generators use. [`toolchain.json`](../toolchain.json) names the
release.

`fbtenv` sets it up. Source it once in every new shell:

- The first time, it downloads the archive for your machine (400–500 MB), checks its
  sha256 against `toolchain.json`, and unpacks it into
  `~/.flipper/flipperone-mcu-toolchain/<tag>/` (or `$FLIPPERONE_TOOLCHAIN_HOME/<tag>/`).
- It points the tree's `.toolchain` link at that release (a junction on Windows). Each
  checkout or worktree has its own link, so trees pinned to different releases work side
  by side. Editors and tools find the toolchain through it without fbtenv.
- Offline: download the archive yourself and point `FLIPPERONE_TOOLCHAIN_ARCHIVE` at it.

After a branch switch to another pinned release, source `fbtenv` again.

> [!WARNING]
> On Windows, `git clean -x` can follow the `.toolchain` junction and delete the release
> from `~/.flipper`; sourcing `fbtenv` reinstalls it. `git clean` without `-x` leaves
> `.toolchain` alone.

### Host requirements

Linux x86_64 and aarch64 with glibc 2.34 or newer, macOS on Apple silicon, and Windows
x64. On Linux the toolchain needs only `libudev1`, which systemd-based distributions
have, plus udev rules to use a debug probe or flash without root (see the toolchain's
[README](https://github.com/flipperdevices/flipperone-mcu-toolchain#host-requirements)).

## Presets and targets

Each preset builds in its own `build/<preset>/`, so switching boards needs no rebuild:

| Preset | Builds |
| --- | --- |
| `f2-release` | f2 firmware (the default board) |
| `f2-recovery` | f2 firmware and `recovery.uf2` |
| `f1-release` | f1 firmware |

Without a preset, pass `-DFW_TARGET=<name>` to CMake. Boards and how to add one:
[Targets.md](Targets.md).

`build/current` links to the preset configured or built last (a junction on Windows).
clangd, the VSCode configurations and `scripts/isr_ram_check.py` use it.

## Toolchain check

Configuring checks the tools against `toolchain.json`: with the toolchain, that it is the
pinned release; without it, that the Pico SDK and GCC versions match. A mismatch stops the
configuration with a hint. `-DFW_TOOLCHAIN_CHECK=WARN` builds anyway, `OFF` skips the
check. Only the toolchain release reproduces CI's binaries exactly.

## Other setups

The [Raspberry Pi Pico extension](https://marketplace.visualstudio.com/items?itemName=raspberry-pi.raspberry-pi-pico)
installs its own Pico SDK and Arm GCC, at the versions on the `sdkVersion`,
`toolchainVersion` and `picotoolVersion` lines of `CMakeLists.txt`, which match
`toolchain.json`. A new build directory uses them only when the pinned toolchain is
neither set up with `fbtenv` nor installed; an existing one keeps the SDK it was
configured with.

An installed toolchain also wins over a global `PICO_SDK_PATH`. Pass
`-DPICO_SDK_PATH=<SDK>` to use another SDK.

## Updating the toolchain

- Pin a new release with `python3 scripts/toolchain/toolchain.py update --tag v<N>` (the
  newest without `--tag`). It reads the release's `release.json` and also rewrites the
  version lines at the top of `CMakeLists.txt`. Do not edit either by hand.
- A change to the build's Python packages (`assets/python/requirements.txt`) needs a
  toolchain release with them first; pin it in the same pull request. Otherwise the build
  ignores the toolchain's Python packages and installs its own from PyPI.
