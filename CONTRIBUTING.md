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
2. Create a working branch from the main branch.
3. Make your changes and verify the build.
4. Flash and test on hardware.

## Commit your update

- Keep commits focused and atomic.
- Write a short summary line, then a body if the change needs explanation.
- Make sure the build is not broken before committing.

## Pull Request

When you're done, open a pull request:

- Fill out the PR template completely.
- Don't forget to [link the PR to an issue](https://docs.github.com/en/issues/tracking-your-work-with-issues/linking-a-pull-request-to-an-issue) if you are solving one.
- [Enable maintainer edits](https://docs.github.com/en/github/collaborating-with-issues-and-pull-requests/allowing-changes-to-a-pull-request-branch-created-from-a-fork) so the branch can be updated for merging.
- As you address review comments, mark each conversation as [resolved](https://docs.github.com/en/github/collaborating-with-issues-and-pull-requests/commenting-on-a-pull-request#resolving-conversations).
- We [may ask for changes](https://docs.github.com/en/github/collaborating-with-issues-and-pull-requests/incorporating-feedback-in-your-pull-request) before merging — apply them and push to the same branch.
- If you run into any merge issues, check out [this git tutorial](https://lab.github.com/githubtraining/managing-merge-conflicts) to help you resolve merge conflicts and other issues.

### Your PR is merged!

Congratulations 🎉🎉 The Flipper Devices team thanks you ✨
