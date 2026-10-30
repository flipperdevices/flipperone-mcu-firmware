---
applyTo: "*.md"
---

# Documentation review rules

The root Markdown files are the only documentation. There is no documentation build, so
this is plain GitHub-flavoured Markdown.

- A change of behaviour, command line, build knob, target or file layout that the docs
  describe must update them in the same PR. Flag a PR that changes behaviour and leaves
  the matching document stale.
- The contribution guide carries the AI-assisted development policy and the one-concern
  rule. Note a PR that contradicts them.
- Document contracts rather than implementation narration: who owns returned memory and
  who frees it, how long a pointer stays valid, which parameters may be null, and what
  counts as success or failure.
- Check that relative links and anchors resolve, and that shell snippets still match the
  build behaviour they describe.
- Licensing is recorded per path in the repository's REUSE configuration. Flag a new file
  that no rule covers, and do not move a file across a licence boundary without saying
  so.
