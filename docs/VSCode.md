# VSCode

## Setup

1. Set up the toolchain once with `fbtenv` ([Building.md](Building.md#toolchain)).
2. Copy the contents of `vscode_template/` to `.vscode/`.
3. Install the recommended extensions: CMake Tools, clangd, Cortex-Debug.
4. Pick a preset in the CMake Tools panel and build.

VSCode finds the toolchain through the tree's `.toolchain` link, with no fbtenv environment.

## Code navigation and formatting

The toolchain's clangd reads the compile commands in `build/current`, as `.clangd` sets, so configure a preset first. On save, clangd formats the changed lines of C and C++ files with `.clang-format`.

## Build, flash and debug

The template's tasks and launch configurations act on `build/current`:

| Entry | Does |
| --- | --- |
| **Compile Project** task | Builds the selected preset |
| **Run Project** task | Builds, then loads the firmware with picotool over USB (BOOTSEL) |
| **Write partition table** task | Builds, then loads `partition_table.uf2` with picotool |
| **Flash** task | Builds, then writes the partition table and the firmware over SWD |
| **Debug**, **Debug (FreeRTOS)** | Build, flash over SWD, stop at `main` |
| **Debug Attach**, **Debug Attach (FreeRTOS)** | Attach to the running firmware, no build |

Debugging details: [Debugging.md](Debugging.md).

## Multi-root workspaces

VSCode ignores `clangd.path` and `clangd.arguments` in a folder's `.vscode/settings.json`: there is one clangd per window. Put both in the `.code-workspace` file, and list this folder first, because the clangd extension expands `${workspaceFolder}` to the first folder:

```jsonc
{
    "folders": [
        { "path": "flipperone-mcu-firmware" },  // first
        { "path": "some-other-project" }
    ],
    "settings": {
        "clangd.path": "${workspaceFolder}/.toolchain/tools/clangd/bin/clangd",
        "clangd.arguments": ["--query-driver=**/arm-none-eabi-*", "--header-insertion=never"]
    }
}
```

The rest of the template's settings work per folder and stay in `.vscode/settings.json`.
