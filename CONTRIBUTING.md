# Welcome to Flipper One MCU Firmware contributing guide

Thank you for investing your time in contributing to our project!

In this guide, you will get an overview of the contribution workflow — from opening an issue to
creating, reviewing, and merging a PR.

## Getting started

Before writing code and creating a PR, make sure that your contribution aligns with the
project's goals and guidelines:

- Your PR must compile and not break the CI build.
- Your PR must pass review by the code owner before merging.
- Keep changes focused — one concern per PR.
- Low-effort AI PRs are not accepted — see [AI-assisted development](#ai-assisted-development).

Feel free to ask questions in issues if you're not sure about the scope or approach.

## AI-assisted development

Using AI tools (Copilot, Claude, Cursor, etc.) is allowed. However, **you are the author
of your PR, not the AI.** This means:

- You understand every change you submit.
- You can explain design decisions without asking an AI.
- You have read and tested the code yourself.
- You can respond to review comments on your own.

### What is a low-effort AI PR

A low-effort AI PR is one where the contributor acts as a relay between the reviewer
and an AI agent — forwarding review comments to the model and submitting its output
without genuine engagement. Signs include:

- Unable to explain why a specific approach was chosen.
- Review responses that restate the question or are clearly AI-generated.
- Changes that don't address the actual feedback — only its surface wording.
- No evidence of testing or local verification.

**Low-effort AI PRs will be closed without merging.** If a PR is identified as such,
the contributor is welcome to reopen it once they have engaged with the code themselves.

The bar is not "did you use AI" — it is "do you understand and own what you're submitting."

## Issues

### Create a new issue

If you find a bug or have a feature request, search existing issues first. If a related
issue doesn't exist, open a new one with enough detail to reproduce the problem or
understand the proposal.

### Work on an existing issue

Check the [MCU Firmware Project tracker](https://github.com/orgs/flipperdevices/projects/8)
for open tasks. Leave a comment on the issue before starting work to avoid duplication.

## Making changes

1. Fork the repository.
2. Create a working branch from `dev`.
3. Make your changes and verify the build.
4. Flash and test on hardware.

## Toolchain

The build tools are the [flipperone-mcu-toolchain](https://github.com/flipperdevices/flipperone-mcu-toolchain)
release pinned in `toolchain.json`; the README shows how to set it up.

- New releases arrive as pull requests from the *Toolchain* workflow, which reads the
  newest release's `release.json`. To pin a release by hand:
  `python3 scripts/toolchain/toolchain.py update --tag v<N>`. It also rewrites the
  version lines the Pico VS Code extension reads at the top of `CMakeLists.txt`.
- A change to the build's Python packages (`assets/python/requirements.txt`) needs a
  toolchain release with them first. Pin that release in the same pull request as the
  change. Otherwise the build ignores the toolchain's Python packages and installs its
  own from PyPI.

## Commit your update

- Keep commits focused and atomic.
- Write a short summary line, then a body if the change needs explanation.
- Make sure the build is not broken before committing.

## Pull Request

Open the pull request as a **draft** against `dev`: CI builds a branch only once it has a
PR, and reviewers, Copilot included, are asked only when you mark it ready. Fill out the
PR template and keep it short: *Verification* lists only steps a reviewer can
realistically repeat, other cases you tried get two or three words each.

Then make these checks as separate passes, not along the way while editing:

1. **Ownership.** For every state, resource and decision the PR touches, make sure that:
   - it has exactly one owner, and that owner is the right module;
   - all its code lives in the owner, and other modules go through the owner without
     reading or duplicating its internal state;
   - no case is handled in two places;
   - no case is left uncovered because a check sits in the wrong place.

   If something is in the wrong place, don't patch around it. Describe the target
   structure (who owns what, what moves where) and agree with the code owner whether to
   widen the PR.
2. **Comments.** Every comment the PR adds or touches is needed, current and short. It
   explains what the code cannot say (a non-obvious reason or constraint); it doesn't
   retell the code next to it or repeat an option's own description.
3. **Documentation.** The same for every passage of documentation the PR adds or touches. User
   documentation follows the user's scenario. The product is in development and its
   procedures, buttons and names still change, so link the single source of truth (the
   page on docs.flipper.net) instead of retelling its steps or UI details.
4. **Hardware.** Run a user-facing flow on hardware the way the user will before claiming
   it works. A procedure quoted in an issue may be stale. In *Verification*, write what you
   actually ran; if it was not the way the user will run it, or not on a device at all,
   say so.
5. **Self-review.** Have the PR reviewed by someone who has not seen your work on it: a
   colleague, or an AI reviewer given only the PR and nothing from your session; it reads
   the diff and whatever code it needs itself. Fix the serious remarks (bugs,
   architecture, misleading documentation; not style or taste) and pass the fixes through
   checks 1-4 again.

Only then mark the PR **ready for review**.

Also:

- Don't forget to [link the PR to an issue](https://docs.github.com/en/issues/tracking-your-work-with-issues/linking-a-pull-request-to-an-issue) if you are solving one.
- [Enable maintainer edits](https://docs.github.com/en/github/collaborating-with-issues-and-pull-requests/allowing-changes-to-a-pull-request-branch-created-from-a-fork) so the branch can be updated for merging.
- As you address review comments, mark each conversation as [resolved](https://docs.github.com/en/github/collaborating-with-issues-and-pull-requests/commenting-on-a-pull-request#resolving-conversations).
- We [may ask for changes](https://docs.github.com/en/github/collaborating-with-issues-and-pull-requests/incorporating-feedback-in-your-pull-request) before merging — apply them and push to the same branch.
- If you run into any merge issues, check out [this git tutorial](https://lab.github.com/githubtraining/managing-merge-conflicts) to help you resolve merge conflicts and other issues.

### Your PR is merged!

Congratulations 🎉🎉 The Flipper Devices team thanks you ✨
