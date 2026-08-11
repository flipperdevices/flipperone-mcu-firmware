# Hardware targets

A target is one board. It is described by `targets/<name>/target.cmake`, and a target
can be based on another one, taking over its sources and replacing only the parts
where the hardware differs.

```
targets/
├── target_api.cmake      the API below, included by the top-level CMakeLists
├── f100/
│   ├── target.cmake      the descriptor
│   ├── config/           furi_config.h, tusb_config.h, ...
│   ├── furi_hal/
│   ├── furi_bsp/
│   ├── src/
│   └── linker_symbols.ld
└── f2/
    └── target.cmake      fw_base(f100), nothing of its own yet
```

The directory name is the target name and must look like `f<number>`; the number is
what `version.c` reports as `TARGET`.

## Building for a target

The default is `f100`. To build another one:

* **VSCode** — `Terminal → Run Task → Select Target`, then compile as usual. Nothing
  else changes: the executable is always named after `project()`, so the compile,
  flash and debug entries never mention a target.
* **Command line** — `cmake .. -DFW_TARGET=f2`.

There is one build directory, so switching targets means a full rebuild. CI builds
every target of the matrix in [.github/workflows/build.yml](.github/workflows/build.yml)
and publishes them as `flipper-one-mcu-<target>-firmware-<suffix>.uf2`.

## What a descriptor is

A descriptor only declares data — which target it is based on, and which sources and
include directories make up the firmware. It must not create CMake targets. That is
what keeps inheritance a matter of editing lists.

Everything that does not vary between boards stays in the top-level
[CMakeLists.txt](CMakeLists.txt): the Pico board, compile definitions, linked
libraries, flash layout, NVM bank size. Move a knob into the API only once a board
actually needs a different value for it.

## Inheritance

`fw_base(f100)` in `targets/f2/target.cmake` makes the search path
`[targets/f2, targets/f100]` — child first. From that one rule:

| To do this | Do this |
| --- | --- |
| Replace an inherited file | Put it in `targets/f2/` under the same relative path. The child's copy wins; the descriptor does not change. |
| Replace an inherited header | The same. Include directories are emitted child first, so `targets/f2/config/furi_config.h` shadows the f100 one while every header f2 did not override keeps coming from f100. |
| Add a file next to inherited ones | Nothing — the inherited glob already covers `targets/f2/furi_hal/*.c`. Only a directory f100 has no equivalent of needs a new `fw_target_sources()`. |
| Drop an inherited file | `fw_target_remove(furi_hal/furi_hal_power.c)` |
| Drop shared code | `fw_remove(lib/drivers/tca6416a/*.c)` |
| Add shared code | `fw_sources(lib/drivers/pcal6416/*.c)` |

A `fw_base()` must be the first statement in a descriptor: the base is loaded in
place, so everything after it is applied on top.

### No quoted includes inside `targets/`

Headers are overridden by include order, not by copying — both directories stay on
the path, child first. Quotes break that: the compiler searches the including file's
own directory first, so one translation unit can pull in the f100 copy and the f2
copy at once. `#pragma once` keys on the path and will not deduplicate them, and
every type in the header is redeclared.

Use `<>` for everything under `targets/`, SDK headers included.

## API

The rule for paths: **without a prefix they are relative to the repository root**,
like everywhere else in the project. The `fw_target_` ones are relative to the target
directory and are looked up along the inheritance chain.

| Declaration | Paths relative to | Effect |
| --- | --- | --- |
| `fw_base(<name>)` | `targets/` | Inherit from another target |
| `fw_sources(<globs>...)` | repository root | Sources shared by every target |
| `fw_target_sources(<globs>...)` | target directory, chain | Board-specific sources |
| `fw_remove(<globs>...)` | repository root | Drop shared sources |
| `fw_target_remove(<globs>...)` | target directory, chain | Drop inherited board-specific sources |
| `fw_includes(<dirs>...)` | repository root | Include directories shared by every target |
| `fw_target_includes(<dirs>...)` | target directory, chain | Board-specific include directories; each expands to every directory in the chain that has it |

Globs are recursive, so `lib/drivers/*.c` also matches subdirectories. The order in
which sources are declared does not matter — the final list is sorted. The order of
include directories does matter and is preserved, with the base's entries first. New
files in an already declared directory are picked up without re-running CMake by hand.

A removal that matches nothing is an error rather than a no-op, so a pattern left
behind by a rename in the base is caught instead of silently changing what is built.

The top-level CMakeLists consumes all of this through three commands:

| Command | Effect |
| --- | --- |
| `fw_load(<target>)` | Load a descriptor and everything it is based on |
| `fw_resolve()` | Turn the declarations into `FW_SOURCES` and `FW_INCLUDES` |
| `fw_find_file(<rel> <out-var>)` | Find a file along the chain, child first — used for `linker_symbols.ld` |

## Adding a target

1. Create `targets/f<number>/target.cmake`. Start with `fw_base(<existing target>)` if
   the board is close to one that already exists, and add nothing else — it should
   build identically to its base before you start changing anything.
2. Add the name to the `target` matrix in [.github/workflows/build.yml](.github/workflows/build.yml),
   and a link line to the body of the `comment` job right below it.
3. Add the name to the `fwTarget` options in `vscode_template/tasks.json` and
   `.vscode/tasks.json` — the two files are kept identical.

## Configure errors

| Message | Meaning |
| --- | --- |
| `Unknown firmware target 'X'. Available: ...` | `FW_TARGET` or a `fw_base()` names a directory that has no `target.cmake` |
| `Cyclic target inheritance: ...` | Two targets are based on each other |
| `fw_remove(X) matched no sources` | The pattern matches nothing. Usually the file was renamed or dropped in the base — fix the pattern or delete the line |
| `'X' not found in any of the target directories` | A file expected by name, such as `linker_symbols.ld`, is missing from the whole chain |
| `FW_TARGET 'X' must follow the f<number> convention` | The directory name is not `f` followed by digits |
