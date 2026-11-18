# MCU PR system qualification

Build calls system-testing's `pr-mcu-smoke.yml` using native `workflow_call`,
pinned to a full commit SHA. It runs inside the MCU Build run and propagates the
hardware job outcome directly into the PR and the aggregate `build` check.
There is no cross-repository PAT, dispatch, polling or separate result receipt.

MCU and system-testing are public, so GitHub downloads the reusable workflow
and shared source action directly. No PAT, GitHub App or cross-repository
secret is required. The shared action materializes its pinned test package
without a cross-repository checkout. Public callers require public reusable
workflows/actions; organization sharing alone cannot enable public → private.
See [GitHub access rules](https://docs.github.com/en/actions/reference/workflows-and-actions/reusing-workflow-configurations#access-to-reusable-workflows).
Both workflow and source-action pins must be updated when adopting new test code.

After the ordinary firmware build, one hardware job and one lease cover two
separate steps. The system step installs the exact `firmware-f2` artifact from
this Build run and one pinned Linux bundle, then runs 25 checks including
standalone MCU embedded units and Linux/MCU I2C/device control. The unit step
runs the Linux-online `unit_tests` command on the same verified pair, without
another flash. Together these steps cover the complete 26-case Nightly MCU
profile, using the same test bodies. There is no additional hardware unit job
for PRs. Non-PR unit builds keep their existing job.

Both selected steps must pass. Missing, skipped, failed or cancelled steps fail
the qualification check. Unit tests still run after a system testcase failure
when preparation can be revalidated against the current lease and live firmware.
The common workflow exposes `mcu_stage: all | system | units` for later splitting
across runners; this PR entry always selects `all`.

The native artifact download uses only the current run's `firmware-f2` artifact.
The preparer checks the internal PR event, full commit, exact artifact filenames,
file bounds, partition-table policy and embedded UF2 revision. It records the
source run/attempt and input hashes before using the nightly A/B converter.
Both live Linux and MCU identities are checked before product tests.

Optional MCU repository variables:

- `FONE_MCU_LINUX_BUILD`: an exact **update-bundle** build number, not rootfs ID.
  Empty selects the latest successful nightly once and pins its number,
  manifests, hashes and runtime identities for this run.
- `FONE_MCU_LINUX_CHANNEL`: defaults to `nightly`, with a 48-hour freshness policy.
  `release-candidate` requires an explicit build number.

No cross-repository secrets or dispatch-ref/workflow variables are required.
Allure HTML and sanitized diagnostics are uploaded to this MCU Build run and
linked from the hardware job summary. PR runs do not publish to S3 or TestOps;
those system-testing repository secrets are not inherited by native callers.
System-testing Nightly keeps its existing publication behavior.

The existing Build trigger covers new/internal PRs, new commits and reopened PRs.
Fork PRs do not enter the hardware path. Build concurrency cancels superseded
native jobs; fo-lab owns cleanup and baseline restoration. The hardware job has
a 110-minute timeout including preparation, testing and reports.

Hosted workflow validation runs pinned, checksum-verified actionlint.
Artifact preparation and qualification regression tests live in system-testing.
Hosted checks alone do not prove physical firmware compatibility.

[Shared MCU automation](https://github.com/flipperdevices/flipperone-system-testing/blob/ssecsd/issue-10-mcu-system-smoke/docs/automation/mcu.md#external-mcu-pr-builds).
