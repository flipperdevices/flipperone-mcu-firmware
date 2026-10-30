---
applyTo: "applications/debug/unit_test_app/**"
---

# On-device unit test review rules

`AGENTS.md` documents how the tests are run. This file is about what to check.

- The suites run on real hardware only, driven over the device's command line. There is
  no host test runner and no build-system test integration: do not propose adding one,
  and do not claim a test passes.
- Tests are registered through a single list. A test function that is not on it never
  runs — check that every added case is reachable and that removing one does not leave a
  dangling entry.
- The runner fails the run when the heap leaked, so every test must free what it
  allocates and reset state between cases. Flag tests that only hold up when run once or
  in isolation.
- Anything that writes persistent storage must clean up after itself, or it leaks into
  later runs on the same device.
- The command under test is driven by an external harness that matches a fixed prompt and
  status line. Renaming the command or changing its output breaks CI — flag it unless the
  harness is updated in the same PR.
- Do not weaken assertions, delete a case, or replace a failure with a log line to make
  the suite pass. A flaky test is a finding, not something to retry away.
