---
applyTo: "targets/**"
---

# Hardware target review rules

`TARGETS.md` is the reference for the descriptor API, the inheritance model and the checklist for adding a board. Read it before commenting and quote it rather than this file when a rule is disputed — it is kept current with the descriptor implementation.

## Descriptors

- A descriptor declares data only: the target it is based on, and the sources and include directories that make up the firmware. It must not create build targets or add custom commands. Anything that does not vary between boards belongs in the top-level build file.
- The inheritance declaration must be the first statement — the base is loaded in place, so everything after it is applied on top.
- Path prefixes follow one rule: some declarations are relative to the repository root and some to the target directory along the inheritance chain. `TARGETS.md` has the table; flag a call that uses the wrong base rather than restating the table here.
- A removal that matches nothing is an error, on purpose. Do not "fix" it by loosening the glob.
- Do not move a knob into the descriptor API until a board actually needs a different value for it.

## Quoted includes are forbidden here

Headers are overridden by include order, not by copying: every directory in the inheritance chain stays on the include path, child first. A quoted include searches the including file's own directory first, so one translation unit can pull in the parent's and the child's copy of a header at once. `#pragma once` keys on the path and will not deduplicate them, and every type in the header is then redeclared.

Flag any quoted include under `targets/`, SDK headers included. It must be angle-bracket form.

## Adding a target

Adding a board touches more than the descriptor. The build workflow's target matrix, the job that posts build links, and the editor task definitions must be updated together; `TARGETS.md` lists them and is the place to check the current set. Also check that a documented target is actually reachable: adding a board to the matrix does not give it hardware coverage, because the on-device test job pins a single target.

## Flash layout

The partition table JSON is the source of truth. The linker fragments next to it are generated from it by a script; flag hand-edits of the generated files — the JSON must change and the generator must be re-run.
