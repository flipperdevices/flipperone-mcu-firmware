# MCU PR system qualification

The Build workflow's `mcu-system-smoke` job calls `mcu-system-smoke.yml` after
the ordinary firmware build. It uses the exact `firmware-f2` artifact from that
run, without rebuilding or downloading a mutable update-server URL.

It dispatches `pr-mcu-smoke.yml` in `flipperdevices/flipperone-system-testing`,
which runs the complete Nightly MCU profile with the PR UF2 and one pinned Linux
bundle. The source PR check waits for that specific run, verifies its result
receipt and links its system run, diagnostics and Allure report. It fails on
setup/test failure, cancellation, skipped/missing tests or timeout. The existing
aggregate `build` check also includes this outcome. PRs no longer run the
separate hardware `unit-tests` job in this repository.

System-testing holds one hardware job, one runner allocation and one lease.
Its system step installs the exact Linux/MCU pair and runs 25 selected checks;
its separate units step executes the Linux-online `unit_tests` command on that
same prepared device, without another flash. Standalone embedded units remain
in the standalone system checks. Together the steps cover the full 26-case
Nightly MCU profile, with distinct step outcomes in one report/PR check.
Both outcomes must be successful; units still run after a system testcase
failure if the same prepared pair can be verified.

The shared system workflow exposes `mcu_stage: all | system | units` for future
separate-runner scheduling. Current PR dispatches always use `all`. A future
standalone units job prepares its own lease instead of reusing another lease's
receipt. Direct MCU unit jobs continue for non-PR builds.

Configure these repository settings before enabling the caller:

- Secret `FONE_SYSTEM_TESTS_TOKEN`: Actions write on system-testing, sufficient
  to dispatch, poll runs/download results, and cancel its own remote run.
- Variable `FONE_MCU_LINUX_BUILD`: required compatible, successful **update-bundle**
  build number, not the rootfs build ID. Select and hardware-qualify this pin.
- Variable `FONE_MCU_LINUX_CHANNEL`: defaults to `release-candidate`. A `nightly`
  pin also has the resolver's 48-hour freshness limit.
- Variable `FONE_SYSTEM_TESTS_REF`: defaults to `main` in system-testing.

In system-testing, configure `FONE_MCU_ARTIFACTS_TOKEN` with Actions read and
Pull requests read on this repository, plus its existing S3 publisher secrets.
Use scoped fine-grained PATs; an App-based setup needs token minting/refresh
because installation tokens expire after one hour. The receiver
workflow must exist on system-testing's default branch before dispatch works.
Do not enable this caller until the receiver is available and the Linux pin and
tokens are configured; missing configuration fails the check explicitly.

Full protocol and setup:
[system-testing MCU automation](https://github.com/flipperdevices/flipperone-system-testing/blob/main/docs/automation/mcu.md#external-mcu-pr-builds).

The caller runs only for internal PRs (including new commits and reopened PRs,
through the existing Build trigger). Fork PRs cannot use the secret/hardware path.
Cancelling a superseded PR build requests remote cancellation; fo-lab owns cleanup
and baseline restoration. The wait budget is three hours including the queue.
No cross-repository token is sent to signed artifact storage URLs.

Local orchestration checks, without network or hardware:

```sh
python3 -m unittest discover -s scripts/tests -p test_system_smoke.py -v
actionlint -shellcheck= .github/workflows/build.yml .github/workflows/mcu-system-smoke.yml
```

These checks validate orchestration, not physical firmware compatibility or a
successful end-to-end dispatch. Configure the receiver, tokens and Linux pin
before enabling end-to-end execution.
