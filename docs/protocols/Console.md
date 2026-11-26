# Console

The MCU's serial consoles and the parts of the CLI that scripts depend on.

> [!NOTE]
> Not written yet. The sections follow [the contract template](README.md#writing-a-contract-document).

- **Counterpart:** `scripts/testops.py`, the agent tools in `scripts/agent_tools`, and people.
- **Source of truth:** the CLI in [`applications/services/cli`](../../applications/services/cli); the commands in [`applications/applications.c`](../../applications/applications.c).

Planned contents:

- Which ports carry a console: USB CDC, the UART to Linux, the debug probe
- The prompt and the status lines scripts match
- Which commands and outputs are stable, and how to change them
