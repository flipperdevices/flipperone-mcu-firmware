---
applyTo: ".github/**,scripts/**,assets/python/**,assets/scripts/**,assets/CMakeLists.txt,CMakeLists.txt"
---

# CI, build wiring and tooling review rules

## Workflows

- Jobs that touch hardware run on the self-hosted runner label declared in the actionlint configuration; everything else stays on a hosted runner. A new self-hosted label must be declared there too.
- Jobs that publish artifacts, post comments or create releases must keep their fork  guard: fork pull requests cannot read secrets. Do not remove the guard.
- Never echo, print or pass a secret on a command line; pass it through `env:` and let the  tool read it from the environment.
- A workflow triggered from the target branch with access to secrets must not check out or execute pull-request code.
- The build workflow's target matrix and the job that posts per-target links must change together with the set of targets.
- Keep the agent environment workflow intact: its file name and job id are fixed by GitHub, and its steps must not install toolchains or run builds. The review runtime is source-only on purpose.

## Agent configuration

- The shared reviewer prompt is `.github/copilot-instructions.md`; the path-scoped files refine it per area. When a directory is moved or renamed, the `applyTo` globs must be updated in the same PR — a stale glob silently stops applying.
- Do not restate project conventions in a new instruction file; extend the existing file that covers the area.

## Scripts and generation

- Helper scripts are called both by CI and by developers. Changing their interface or output format breaks whatever parses them, so the script and its callers must move together.
- Python tooling uses the repository's pinned virtualenv and requirements file. Do not add a second environment, an unpinned dependency, or an install step inside a build.
- Keep generation deterministic: the same input must produce the same output on a rebuild, with no timestamps or absolute paths baked in.
- Derived files (flash layout, generated sources) come from a declared input. Change the input and regenerate rather than editing the output.

## Build wiring

- Sources are collected by directory globs, so a new file in an existing directory is picked up automatically, while a new directory or a source outside the expected trees needs an explicit entry. Never glob over a submodule — an unfetched submodule makes the glob expand to nothing instead of failing.
- Build outputs and generated intermediates are ignored by version control. Flag a commit that adds them.
