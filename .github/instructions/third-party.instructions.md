---
applyTo: "lib/corelibs/**,lib/freertos/**,lib/nanopb/**,lib/pico-kvstore/**,lib/rtt/**,lib/sdk/**,assets/proto/**,applications/services/gui/clay.c,applications/services/gui/clay.h,applications/debug/unit_test_app/minunit.h"
---

# Third-party, submodule and generated-code review rules

## Telling third-party from first-party

`.gitmodules` records which directories are submodules. Everything else under `lib/` that is maintained in this repository — the toolbox, CLI engine, containers, chip drivers, PIO and USB glue — is first-party and is reviewed like the applications.

A few third-party files are vendored in-tree without a submodule, such as the GUI layout library and the on-device test framework header. Review our use of them in the code that drives them, not the libraries themselves.

## Submodules

- A submodule pointer change produces no file diff. Do not infer what changed inside it: ask for a link to the upstream pull request or commit range, and check that the bump is justified in the PR description.
- Focus findings on our integration — build wiring, hand-picked source lists, and any glue we add — and do not suggest broad refactors, reformatting or renames in a vendored tree.
- Some source lists are written out by hand instead of globbed, because a glob over an unfetched submodule expands to nothing instead of failing. Do not "simplify" one back into a glob.
- If `.gitmodules` and the working tree disagree about a path, that is a finding: report the mismatch instead of assuming which side is right.

## Generated files

Protobuf output, generated image sources, the asset header, and the flash-layout fragments derived from the partition table are all produced by the build. Never ask to hand-edit them — the generator or its input must change instead.

Schema changes belong in the proto repository and arrive here as a submodule bump. If a wire-format guard asserts the generated-header version, a bump must update that assertion deliberately and revisit the code that builds the wire header in the same PR.

## Licences

Licensing is recorded per path in the repository's REUSE configuration, and vendored code keeps its upstream licence. Do not ask for a licence header to be added inside a vendored file.
