# Debugging

> [!NOTE]
> This document is incomplete. Planned: probe setup and wiring, RTT log channels, reading
> crash reports, common debugger pitfalls.

## Logs

- **MCU console:** the `log` command streams the log; `dmesg` prints what was logged since
  boot.
- **RTT:** the VSCode debug configurations show the log as the *Flipper One Logs* channel.

## Debugger

The VSCode **Debug** configurations ([VSCode.md](VSCode.md#build-flash-and-debug)) use
the toolchain's OpenOCD and GDB with a CMSIS-DAP probe. The OpenOCD targets for the two
cores are `rp2350.cm0` and `rp2350.cm1`.

Cores left halted after a debug session are a common reason for a device that seems
frozen. The template makes OpenOCD resume both cores when GDB detaches:

```json
"rp2350.cm0 configure -event gdb-detach { resume }",
"rp2350.cm1 configure -event gdb-detach { resume }"
```

## Crashes

The firmware does not reboot after a crash: it prints a `[CRASH]` report to the log and
halts. Nothing of it survives a reset, so keep a log running while testing something that
may crash. [AGENT_TOOLS.md](../scripts/agent_tools/AGENT_TOOLS.md#when-the-firmware-crashes-or-hangs)
describes the reports and how to turn their addresses into source lines.
