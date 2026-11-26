# Testing

## On-device unit tests

The firmware's unit tests (`applications/debug/unit_test_app`) run on a real device only,
driven over its console. CI runs them on a self-hosted stand after every build:

```shell
python3 scripts/testops.py sanity --commit <sha> --target f2   # the running build is <sha>
python3 scripts/testops.py unit-tests                          # runs the unit_tests command
```

Both need a leased stand (`FO_LEASE` or `/run/fo/lease.json`), which exists only on the
`fone-lab` runners. On your own device, run `unit_tests` in the MCU console instead.

- Register every test in the suite's list; a test missing from it never runs.
- The run fails when the heap leaked, so each test frees what it allocates.
- `testops.py` matches the command's prompt and status line. Changing the command's name
  or output breaks CI unless the script changes with it.

## Host tests

These need no device and no firmware build:

| Suite | Command | Needs |
| --- | --- | --- |
| Agent tools | `python3 -m pytest scripts/agent_tools/tests -q` | `scripts/requirements-test.txt` |
| Toolchain scripts | `python3 -m pytest scripts/toolchain/tests -q` | pytest |
