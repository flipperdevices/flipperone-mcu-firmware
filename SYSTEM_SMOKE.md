# MCU PR system qualification

Build's `mcu-system-smoke` job calls the local reusable MCU workflow after
firmware builds. A hosted caller downloads this run's immutable `firmware-f2`
artifact and dispatches `pr-mcu-smoke.yml` in system-testing. The hardware run
there reuses Nightly MCU's full selection and returns an exact result receipt.
The source check links that run, diagnostics and Allure HTML, and propagates
its status into the aggregate `build` check.

One hardware job and one lease cover separate system and unit steps: 25 checks
for installation, standalone embedded units, MCU/Linux identities, I2C exchange
and LED control, followed by the Linux-online embedded unit command. The unit
step uses the same prepared pair without another flash. A failed candidate
installation or readback verification stops further hardware cases and blocks
units. A failed product case after valid preparation still permits units.
Both selected step outcomes must pass; missing/skipped outcomes fail the gate.
PRs do not schedule the older separate hardware unit job; non-PR builds retain it.

The caller sends source run/attempt, PR head, artifact ID and exact UF2/table
hashes. The receiver verifies the internal source PR/build and artifact
ownership before conversion and flashing. One successful nightly Linux bundle
is selected and pinned by build, manifests, runtime identities and hashes.
The result receipt binds both stage outcomes to the same source build and UF2.
Cancellation/timeout requests cancellation of the particular remote run.
Fork PRs are excluded from the secret/hardware path.

Configure the same fine-grained PAT as Actions secret `MCU_PR_TEST_TOKEN` in both
repositories. Owner: `flipperdevices`; selected repositories: MCU firmware and
system-testing; `Actions: write` (includes read). A single token applies these
permissions to both selected repositories. It dispatches/polls/cancels in
system-testing and downloads the MCU artifact. No token is forwarded to signed
artifact storage URLs or embedded in request JSON.

Optional MCU repository variables:

- `FONE_MCU_LINUX_BUILD`: exact update-bundle number; empty selects latest successful nightly once.
- `FONE_MCU_LINUX_CHANNEL`: defaults to `nightly`; RC requires an explicit build.
- `FONE_SYSTEM_TESTS_REF`: receiver ref, normally `main`.
- `FONE_SYSTEM_TESTS_WORKFLOW`: normally `pr-mcu-smoke.yml`.

Until the receiver is merged, use `FONE_SYSTEM_TESTS_REF=ssecsd/issue-10-mcu-system-smoke`
and `FONE_SYSTEM_TESTS_WORKFLOW=nightly-mcu.yml`. That registered workflow accepts
`mcu_request` on the draft ref and gives its run the `PR MCU smoke` request name.
After merge remove those two overrides. Both routes select `mcu_stage=all`.
GitHub requires a dispatched workflow to exist on the default branch.

The hardware job and Allure report appear in system-testing Actions. Its existing
S3 credentials publish HTML; PR reports do not restore nightly history or upload
to TestOps. The MCU check links the particular system run and result. The shared
workflow retains `mcu_stage: all | system | units` for future separate-runner use.

Hosted validation runs pinned/checksum-verified actionlint and offline tests of
request correlation, stale/missing/failed receipts, cancellation and nightly
selection. Hardware validation is separate.

[MCU automation](https://github.com/flipperdevices/flipperone-system-testing/blob/ssecsd/issue-10-mcu-system-smoke/docs/automation/mcu.md#external-mcu-pr-builds).
