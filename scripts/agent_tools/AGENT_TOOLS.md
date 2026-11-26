# Flipper One hardware tools for LLM agents

`scripts/agent_tools/f1.py` lets you see the screen of an attached Flipper One, press its buttons, read its logs, run commands on its MCU and its Linux host, and reflash it, through a Flipper One Debug Probe. This file covers how, and where the limits are.

```shell
python3 scripts/agent_tools/f1.py <command> --help
```

- **Terms:** the **MCU** is the RP2350 running this firmware. The **host** is the Linux processor inside the Flipper, powered by the MCU; `cpu_app_start` and `cpu_reset` refer to it. **This machine** is the computer you run commands on.
- **Paths** here are relative to the repository root. `f1.py` runs from any directory; file arguments (`OUT.png`, `FILE.bin`, `--out FILE`) are relative to your working directory.
- **Behaviour:** every command is non-interactive, has a timeout (except `log` without `--seconds`), prints plain text, and on failure exits non-zero with a one-line `error:`.
- **Requirements:** Python standard library on macOS and Linux; Windows needs `pyserial` and is untested. Everything here was exercised from macOS on a Flipper One rev. F0B1C2.

**Probe firmware 0.7.0 or newer** is required ([releases](https://github.com/flipperdevices/flipperone-debug-probe)). Update a 0.7.0+ probe with `f1.py probe flash FILE.uf2 --yes`. An older probe shows the serial `flip_one_debug` instead of `f1_debug_xxxx_` in `python3 scripts/get_port.py`, and every device command fails saying so. To update it, ask the user to hold the probe's `MCU RESET & PROBE BOOT` button while plugging in its USB cable, then copy the `.uf2` onto the `RP2350` drive.

**Not done or not verified** — treat as unknown, not working:

- Port lookup on Linux and Windows (follows those systems' CDC port naming, never run there).
- Probe selection on Windows: `F1_PROBE` has no effect, set `F1_PORT_<ROLE>` instead.
- Flashing the host through maskrom (entering maskrom works).
- The `Maskrom` item in the MCU Settings menu.
- Refusing a probe older than 0.7.0 (no such probe was left to try).

## Start of session

```shell
python3 scripts/agent_tools/f1.py status
```

Shows the probe and its firmware, connected ports, whether the MCU runs or is halted, the active slot and each slot's contents. For Linux, run `f1.py linux state`.

## Commands

| Command | What it does |
|---|---|
| `status` | Probe firmware, ports, MCU state and uptime, firmware slots. MCU state and slots come over SWD; `--no-swd` skips them. If a debugger holds the probe, the serial part is printed and the command fails. Its `linux` line only guesses from the `mcu-usb` port; use `linux state` |
| `ports` | Serial ports by role |
| `screenshot OUT.png` | Device screen, 258x144 grayscale, saved 3x enlarged |
| `press BUTTON...` | Click buttons in order, 0.15 s apart. `--shot OUT.png` adds a screenshot in the same session |
| `press BUTTON --hold S --repeats N` | Long press: `press`, `long` after `S` s, `N` `repeat` events 50 ms apart (default 0), `release`. Several buttons are held one after another, never together |
| `press BUTTON --action press` | One raw event: `press`, `release`, `short`, `long` or `repeat` |
| `touch X Y [--to X Y]` | Tap or swipe: x 0..1023 left to right, y 0..767 top to bottom, `--pressure` 0..16383 (default 8000). A swipe is 10 moves 20 ms apart (`--steps`). Takes `--shot` |
| `cli "CMD"...` | MCU console commands, e.g. `cli uptime "power info"`. `--answer y` answers a `y/n` question (as `otp program` asks). `--raw`: after syncing to a prompt, send each argument as bytes (escapes like `\r`, `\x03` work), add no Enter, await no prompt, and print the reply until 0.5 s of silence or `--timeout` |
| `probe "CMD"...` | Commands of the debug probe's own console, e.g. `probe device_info` |
| `probe flash FILE.uf2 --yes` | Reflash the probe and wait until it is back |
| `log --seconds N` | MCU log, each line stamped with this machine's time. `--out FILE` appends, flushed per line. Without `--seconds` it runs until killed |
| `linux state` / `start` / `stop [--hard]` | Query, boot or shut down the host |
| `host "CMD"` | One shell command line on the host's serial console (chain with `;` or `&&`; nothing interactive); prints output and exit code. `--wait S` first waits up to `S` s for a prompt |
| `swd status` / `reset` / `resume` / `read ADDR [N]` | SWD through OpenOCD; no halt unless stated. `read` prints `N` 32-bit words |
| `swd where` | Halt briefly, print where both cores are (function, file, line), run on |
| `swd symbol ADDR...` | Function, file and line for crash-log addresses, from the newest ELF under `build/`; no probe needed |
| `swd flash a\|b FILE.bin --yes` | Write an image to a slot, verify, reset |
| `swd flash a\|b FILE --activate --yes` | Same, then boot the try-before-you-buy image once so the bootrom stages and commits it |
| `swd erase-header a\|b --yes` | Make a slot unbootable so the other boots, and reset. Only reflashing the slot restores it |

- **Buttons:** `ok back up down left right key1 key2 key4 key5 power sw ptt`. `power` is the key in `key3`'s place. What a button does depends on the app on screen; `sw` opens the app switcher in Linux.
- **Timeouts:** `cli` and `probe` 5 s per command, `host` 30 s, `linux start` 120 s, `linux stop` 90 s; `--timeout SECONDS` changes them. For `host` it counts from the command's start (after `--wait`), and a command that timed out keeps running until the next `host` call interrupts it. `screenshot`, `press` and `touch` give up after a few seconds; `swd` after 2 minutes, `swd flash` after 5.
- **Exit codes:** 0 on success, 1 on a tool error. `host` exits with the command's code and prints `[exit code N]` when it is not 0; a tool failure prints `error:` on stderr instead, which tells the two apart.
- Commands that need `--yes` refuse without it and change nothing. Write addresses with `0x`.

### Machine-readable output

`ports`, `status`, `swd status`, `swd read` and `linux state` take `--json` and print one object:

```json
{"tool": "f1.py", "command": "linux state", "ok": true, "data": {"state": "up", "app": "cpu_app_start"}}
```

On failure `ok` is false, `error` replaces `data`, the object goes to stderr, and the exit code is 1. Other commands reject `--json`.

## Rules

- **An unchanged screenshot does not prove a press was ignored**: some actions take seconds to show. Before repeating a press that powers off, confirms or deletes, check another signal (`linux state`, `log`, `host`).
- **Use the cheapest signal that answers the question.** `linux state`, `cli "desktop get_run_app"`, `status` and `log` are cheap text; a screenshot costs an image. Take one when the answer is only on screen: before pressing buttons on an unseen screen, or when the screen is what you check.
- **Never use `probe-rs` for SWD.** One-shot `probe-rs` reads left the device frozen until a debugger attached and detached; cause unknown.
- **The probe takes one SWD client.** Only `swd` commands and `status` without `--no-swd` use SWD; the rest uses serial ports and works during a debug session. While a debugger holds the probe, SWD commands fail within seconds with `The probe is probably held by another debugger` (OpenOCD: `CMSIS-DAP command CMD_INFO failed`). Not a fault: ask the user, or do without SWD.
- **Run `host sync` before cutting host power**: `cli "desktop stop_app"` while the host app runs, `Stop` or `Reboot` in the MCU power menu, `cli "power off"`, `"power ship"`, `"power reboot"`. `linux stop --hard` syncs for you (it is `sync` plus `desktop stop_app`). An MCU reset (`swd reset`, `swd flash`) does not cut host power; see [MCU reset while Linux runs](#mcu-reset-while-linux-runs).
- **Check each step before the next**: read the state back after every action, and stop if it is not what you expected.
- **Ask before destructive actions**, unless the user said the device is free to use. `--yes` tells the tool they agreed. Destructive:
  - `swd flash`, `swd erase-header`, `probe flash`;
  - `linux stop --hard` and anything else in the previous rule that cuts power under a running Linux;
  - switching the device off or sending the MCU to its bootloader: `cli "power off"`, `"power ship"`, `"power boot"`, `Power Off` in the MCU power menu.

  No need to ask before `linux stop`, `swd reset`, an MCU reset through the probe, `probe "power reboot"`, or entering and leaving maskrom with the host off.
- **Measure time with `log` timestamps** (this machine's clock). MCU timestamps lose time; see [Pitfalls](#pitfalls).
- **If what you observe contradicts the user, the user is right**: they are looking at the device.
- **Report what you verified, what you did not, and the state you leave the device in.**

## Seeing and pressing

Screen and input go through the MCU's RPC protocol ([`assets/proto/rpc.proto`](../../assets/proto/rpc.proto)), opened with the MCU console's `rpc` command. Each `screenshot`, `press` or `touch` opens and closes its own session.

- The screen is whatever the display shows: the MCU desktop, the host's boot menu, or the host's panel UI once Linux is up.
- A screenshot takes about a second. Starting a capture makes the MCU redraw, so you always get the current screen.
- A click sends `press`, `short`, `release`, as the firmware does. For physical buttons the firmware derives `short`, `long` and `repeat` from the hold time; for injected events it does not, so the tool sends them. Apps choose which event they react to; most of the MCU UI uses `press`. Firmware older than the `SHORT`/`LONG`/`REPEAT` extension logs `Unknown action` for `short` and ignores it; clicks still work through `press`.
- Menus scroll on `press` and `repeat`; to move several items, repeat the button: `press down down down`.
- Some screens need a held button. The Keypad Test (Settings, Testing, Keypad) ignores a short Back and exits on Back held for nine repeats: `press back --hold 0.5 --repeats 9`.
- Touch events are delivered as is; what a point means is up to the app. The Touchpad Test maps only the central 70% × 60% of the range onto its drawing area.

**MCU apps:** one runs at a time; the desktop is not an app. `cli "desktop get_run_app"` names it (`no app is running` on the desktop), `cli "desktop list_apps"` lists all, `cli "desktop start_app <appid>"` and `cli "desktop stop_app"` switch. `stop_app` closes any app without buttons, but if the app is `cpu_app_start`, it cuts host power.

**MCU desktop:** `key2` Help, `power` the power menu, `key4` Settings. In menus `up`/`down` move, `ok` selects, `back` leaves. The `power` button always belongs to the MCU, even while Linux owns the rest of the screen and buttons. The power menu depends on the host:

- Linux off: `Start Flipper OS` and `Power Off`. `Power Off` puts the whole device in ship mode; only the user brings it back, by holding the power button.
- Linux running: `Stop` and `Reboot`, which act on the host, not the MCU.

## Linux host

Linux does not start by itself after the MCU boots, after a shutdown, or after a host `reboot` (it then waits in the boot menu).

| Command | What it does | Typical time |
|---|---|---|
| `linux state` | `off`; `powered, no shell prompt`; `up: shell answers`; `up: shell answers, but the MCU host app is not running`; `powered in maskrom mode` | 1 s |
| `linux start [--entry N\|NAME]` | Powers the host, lists the boot entries, picks one (default 1), waits for the shell | 30–45 s |
| `linux stop` | `sync`, then `poweroff` in Linux; waits until the MCU reports the host off | 7–20 s |
| `linux stop --hard` | Tries `sync`, then the MCU cuts host power without warning Linux | 3 s |

- Prefer `linux stop`; use `--hard` to test power loss or when the host no longer answers. None of these needs a screenshot: every step is confirmed on the consoles.
- **Self-check:** `linux start` fails while another MCU app runs, usually `self_check`. It runs at every MCU boot and stays on screen when a check failed (a device not found, say). `press back` or `press ok` closes it; `press back` does nothing on the desktop. The first time it stays open in a session, take a screenshot, close it, carry on, and tell the user in your next message; after that, do as they say.
- **Entries:** `linux start` prints the entries numbered as in the on-screen menu, e.g. `boot entry 7: Minimal-7.4.0-rc1-g6cd481aae87d`. `--entry` takes the number, or text that exactly one entry name contains, case-insensitive: `--entry router-7`. No match or several matches fail with the list printed and the host powered off again: a way to list an unknown image's entries.
- **`powered, no shell prompt`** is a host still booting, waiting in the boot menu (after a host `reboot`), at the U-Boot prompt, or at a login prompt. Give a booting host time with `host "true" --wait 60`; in the boot menu, pick with `press down ... ok`. `linux start` says `host is already powered` only while the MCU host app runs. If the host stays without a prompt, `linux stop --hard` and `linux start`.
- `linux stop` also works without the MCU host app: it takes the kernel's `Power down` line as the end.
- If `linux start` sees no sign of life, the MCU lost track of host power: `swd reset` (then `press back` if the self-check stays open) and start again.
- An entry that exists but does not boot makes `linux start` fail with U-Boot's message, leaving the host at the U-Boot prompt; `linux stop --hard` gets out.

**The host shell:** `host "CMD"` works without network. The console is logged in as `user`; `sudo` needs no password. At a login prompt `host` fails with `shows no shell prompt`; logging in is not automated. The image build is in `/etc/os-release` (`BUILD_GIT`, `BUILD_ID`), the kernel in `uname -r`. For files use SSH, `user@<address>`, password `user`; `host "hostname -I"` prints the address (nothing without network).

### How start and stop work

- The MCU powers the host while its app `cpu_app_start` runs: `cli "desktop start_app cpu_app_start"` starts it, `cli "desktop stop_app"` cuts power. When Linux powers itself off, the app exits by itself.
- U-Boot scans for images for several seconds, prints `(N bootflows, N valid)` on the Linux console, lists the entries (`/boot/loader/entries/*.conf`) there in menu order, then shows the menu on screen and waits (no timeout within 70 s). It takes keys from the Linux console (arrows, Enter) and from the device buttons.
- `linux stop` waits for the kernel's `reboot: Power down` and types nothing meanwhile. Do the same by hand: Ctrl+C on the Linux console interrupts the pending `poweroff`.
- systemd ignores `poweroff` while still starting up, which lasts tens of seconds after the shell first answers; `linux stop` resends it.

### Maskrom mode

`cli "desktop start_app cpu_app_maskrom"` powers the host with its maskrom line held for a second, so the SoC stays in its boot ROM and waits for a loader on its own USB port. `cli "desktop stop_app"` powers it off; a normal `linux start` works afterwards.

- Signs: `CPU app started with arg: maskrom` in the MCU log, a silent Linux console (a normal start prints DDR training and U-Boot within a second), a black screen, `linux state` = `powered in maskrom mode`.
- Flashing the host this way needs a USB cable from the host's port to this machine and Rockchip tooling; not tried.

### MCU reset while Linux runs

Resetting only the MCU (`swd reset`, `swd flash`, the probe's `mcu_reset`, a crash) leaves Linux running: an I/O expander that keeps its outputs holds the host power rails. Then:

- About 1.5 s after boot the MCU sees the powered host on the expander and starts the host app; nothing restarts and the self-check is skipped. The screen shows "Waiting for CPU screen..." until Linux sends a frame. The MCU raises its restart interrupt and the Linux driver writes its settings back (`MCU restarted, restoring its settings` in the kernel log).
- `linux start` attaches to the running Linux: the host app leaves a powered host alone and `--entry` is ignored. The tool runs `sync` first and checks that the host uptime went on; if the host restarted anyway, it says so.
- Rarely the MCU fails to initialise the expander, as the bus is shared with the host (see [Pitfalls](#pitfalls)): `Failed to initialize Main Expander` in the log, `Not initialized Main Expander` in `cli dmesg`. Until the next reset the MCU cannot control host power; `linux start` detects it and refuses. Reset the MCU again and retry. Linux is not disturbed.
- By hand: `Start Flipper OS` in the power menu, or `cli "desktop start_app cpu_app_start"`.

### By hand, through the screen

Only when the screen, buttons or UI are under test; it needs screenshots, the commands above do not.

- **Start:** `press power --shot s.png` shows `Start Flipper OS` selected; `press ok`; after about 10 s a screenshot shows the U-Boot menu; `press down ...`, `press ok`.
- **Clean stop:** on the host panel `press key2` (Menu), `Settings`, scroll to `Shutdown` (last, after `Reboot`), `press ok`. There is no confirmation. The screen keeps the list, then goes black for several seconds; neither means the press failed.
- **Hard stop:** with Linux running, `press power` shows `Flipper OS` with `Stop` and `Reboot`; `ok` on `Stop` cuts power within a second.

A hard stop can log `Bq2579x` errors in the MCU log (`Failed to write reg 0x01`, `Failed to set charge voltage limit!`), not every time. The stop path resets the charger and the USB PD controller; the shared I²C bus ([Pitfalls](#pitfalls)) is a likely but unconfirmed cause.

## Ports

| Role | What | Baud |
|---|---|---|
| `mcu` | MCU console through the probe's UART; always there, the tools use it | 1500000 |
| `mcu-usb` | The same console on the MCU's own USB: needs a cable to the Flipper's USB port, and exists only while the host is off (the port switches to the host when it is powered) | any |
| `mcu-log` | MCU log, output only | 1500000 |
| `linux` | Linux serial console | 1500000 |
| `probe` | The debug probe's own console | any |

- `--console mcu-usb` (for `cli`, `screenshot`, `press`, `touch`) uses the USB console, which vanishes mid-command when Linux powers up.
- A present `mcu-usb` means the host is off; an absent one means nothing (maybe no cable). Use `linux state`.
- The probe's UART bridges run at 1500000 baud whatever this machine asks, unless custom rates are enabled: `f1.py probe "settings set_custom_baudrate 1"`.
- The UART consoles print nothing when a port opens; a prompt appears only after input. The tools send Ctrl+C and Enter first.
- **Who uses which port:** `cli`, `screenshot`, `press`, `touch` → `mcu`; `log` → `mcu-log`; `host` → `linux`; `linux` → `mcu` and `linux`; `probe` → `probe`; `status` → `probe` and SWD; `swd` → SWD only. A background `log` blocks only another `log`.
- **Lookup** is in `scripts/agent_tools/transport/ports.py`; `python3 scripts/get_port.py mcu` prints a path, without arguments it lists all roles. Ports are matched by USB identity and CDC control interface: probe `37c1:f131`, serial `f1_debug_xxxx_`, interfaces 1 `linux`, 3 `mcu`, 5 `mcu-log`, 7 `probe`; MCU `37c1:f101`, serial `flip_one_mcu`, interface 0. macOS names a port after the data interface, one higher: `..._2` `linux`, `..._4` `mcu`, `..._6` `mcu-log`, `..._8` `probe`. Override with `F1_PORT_<ROLE>`, e.g. `F1_PORT_MCU_LOG=/dev/ttyACM2`.

### Several probes

Each probe's serial `f1_debug_xxxx_` carries four hex digits folded from its chip ID; the full ID ends the USB product string (`Flipper One Debug Probe 63E7482F2DB3DF2B`). On macOS the serial is in the port name: `/dev/cu.usbmodemf1_debug_d950_4`.

- `ports` lists attached probes with IDs and serials.
- With several probes, every command stops and lists them until you choose: `F1_PROBE=d950 python3 scripts/agent_tools/f1.py status`. `F1_PROBE` takes the four digits, or the chip ID or its start. The choice covers serial ports and SWD (OpenOCD gets `adapter serial f1_debug_xxxx_`).
- `mcu-usb` is not tied to a probe; with more than one board on USB it is left out unless `F1_PORT_MCU_USB` is set.
- If a cable may have moved, confirm the board: `cli device_info` prints the MCU's hardware UID, the Linux prompt shows the hostname.

### The probe's console

`f1.py probe "CMD"...` runs commands on the probe itself:

| Command | Use |
|---|---|
| `device_info` | Probe firmware version, commit and chip ID, in the MCU's `device_info` format |
| `gpio mcu_reset 0`, then `1` | Hold the MCU in reset, then release it |
| `power boot` | Reboot the probe into its USB bootloader; an `RP2350` drive appears, a `.uf2` copied onto it flashes the probe. `f1.py probe flash` does all of it |
| `power reboot` | Restart the probe |
| `help` | All commands |

- An MCU reset through the probe needs no SWD, so it works while a debugger holds the probe: `f1.py probe "gpio mcu_reset 0" "gpio mcu_reset 1"`. Linux keeps running, as with `swd reset`.
- The probe's `power boot` and `power reboot` share names with the MCU's but leave the MCU alone.
- `gpio` also lists `cpu_reset` and some debug pins whose purpose changes between firmware versions; do not rely on them.

## MCU console

| Command | Use |
|---|---|
| `uptime` | Seconds since MCU boot: a quick "did it reset?" |
| `device_info` | Firmware branch, commit and version actually running |
| `dmesg` | The last 10 KB of the MCU log, in RAM. Starts when the MCU apps start (around `Starting self check autorun`), so earlier boot lines are never in it; old lines are overwritten. For looking back; `f1.py log` captures from now on |
| `power info` | Battery, USB and charger readings, once |
| `power show` | The same in detail, repeated until interrupted; under `cli` it ends in a timeout error after printing |
| `help` | All commands |

**Power commands**, only when they are the point of the test:

- `power reboot` cuts power to the whole device, host included; it comes back by itself within seconds.
- `power off` and `power ship` switch the whole device off. After `ship` it returns when the user holds the power button; after `off`, only when USB power is connected.
- `power boot` resets the charger and USB PD controller and reboots the MCU into BOOTSEL: the console goes silent, `status` shows `active none`, and with a cable on the Flipper's USB port an `RP2350` drive appears. `swd reset` or a probe reset brings the firmware back. A running Linux stays up.

A console left in a bad state (a command still running, a crashed RPC session) is recovered by the tools on the next call. If they report no prompt, the MCU itself is down: see [When the firmware crashes or hangs](#when-the-firmware-crashes-or-hangs).

## Firmware slots and SWD

**OpenOCD** must support the RP2350; the tools take `F1_OPENOCD`, else the toolchain's (set up by `fbtenv` in this shell, or the tree's `.toolchain` link), else the Pico extension's (`~/.pico-sdk/openocd/<version>/openocd`). To call it yourself: `-f interface/cmsis-dap.cfg -c "adapter serial f1_debug_xxxx_" -f scripts/agent_tools/openocd/flipperone-mcu.cfg`. The board config sources `target/rp2350.cfg`, sets the adapter speed and holds the board knowledge, so debug sessions can reuse it:

| Proc | Use |
|---|---|
| `f1_where F1>` | PC and LR of both cores after a brief halt: `F1> <core> <pc> <lr>` per core |
| `f1_resume` | Halt both cores and run them again; frees cores a debugger left halted |
| `f1_read ADDR COUNT F1>` | `COUNT` 32-bit words as `F1> <word> ...`, without halting |
| `f1_halted` | 1 if the selected core is halted, else 0 |
| `f1_stage_tbyb BASE` | Boot the try-before-you-buy image at `BASE` once, so the bootrom stages and commits it |

`swd symbol` uses the toolchain's `arm-none-eabi-addr2line`, else the first on `PATH`, else the Pico extension's.

**Slots:**

- Two slots, A and B; the bootrom boots the valid image with the higher version, A on a tie. Slot layout and recovery: [docs/Flashing.md](../../docs/Flashing.md).
- `build/<preset>/flipperone-mcu-firmware.bin` runs from either slot; `swd flash` writes it at the slot's address. A `.uf2` is accepted only if its payload lies inside that slot, i.e. a slot-relocated `.uf2` from the release flow, not a plain picotool `.uf2` linked for A. Anything outside is refused before flash is touched, except picotool's boot-info block at `0x10FFFF00`, which the bootrom ignores.
- **Flash your builds to slot A with `--activate`.** Local builds share one version, so a build in B does not start while A holds a valid image of the same or a higher version. If B holds a higher version than your build, write both slots, or `swd erase-header b`.
- With Linux running, add `--quiesce`: the probe holds the host CPU in reset during the write. That resets a running host (it loses its state), but the write cannot race the host reading flash.
- `status` shows each slot as `erased`, `no image header`, or `valid` with two version words. The first packs `FW_IMAGE_MAJOR` (bits 15..12), `FW_IMAGE_MINOR` (11..4) and `FW_IMAGE_PATCH` (3..0) from `CMakeLists.txt`: 0.1.0 is `0x0010`. The second is reserved for a boot counter, 0 today. `DIST_SUFFIX` changes only the string `device_info` prints.
- `active none`: the core is in the bootrom, because no slot is bootable (flash slot A over SWD) or `power boot` sent it to BOOTSEL (reset it).
- Useful `swd read` addresses: `0x400d0034` active slot (low 12 bits = slot offset / `0x1000`: `0x002` A, `0x782` B, 0 if nothing booted); `0x400b0028` µs since MCU boot; `0xE000EDF0` debug status (bit 17 = halted); `0x1c000000 + offset` raw flash.

### Build, flash, check

```shell
DIST_SUFFIX=my-test-1 cmake --preset f2-release && cmake --build --preset f2-release   # own version string
python3 scripts/agent_tools/f1.py swd flash a build/f2-release/flipperone-mcu-firmware.bin --activate --yes
python3 scripts/agent_tools/f1.py status              # slot A is active
python3 scripts/agent_tools/f1.py cli device_info     # the running build is yours
python3 scripts/agent_tools/f1.py press back          # close the self-check, if it opened
```

- `swd flash` takes about 8 s. The console answers within a second after `swd flash`, `swd reset` or a probe reset; no wait is needed.
- An image with a lower version than the other slot is not chosen: always confirm with `status` and `device_info`.
- With Linux running, `swd flash` (with or without `--activate`) resets only the MCU, which shows the running Linux again by itself; see [MCU reset while Linux runs](#mcu-reset-while-linux-runs).

### Try-before-you-buy images

Builds use `PICO_CRT0_IMAGE_TYPE_TBYB`: the bootrom treats a new image as an unbought purchase.

- A normal reset skips it: the slot stays inactive and `status` keeps showing the other one, as after a plain `swd flash`, which warns about it (written and verified, but not booted until staged).
- `--activate` writes the slot, then resets into the bootrom's FLASH_UPDATE path, which runs the image so the firmware's updater service buys it. It waits up to two minutes for the header to come back bought and fails otherwise, so success means the slot really boots. It costs a few seconds more. The reboot goes through `f1_stage_tbyb`, usable by hand in a debug session.
- That flash-update boot leaves a running Linux running: the new firmware finds the host powered and starts the host app (`Host is powered: starting CPU app`). A real update differs: the old firmware cuts host power before switching, and the new one powers the host again after a reboot (`Switched to a new image: starting the host`) or leaves it off after a power-off.
- Committing the staged image invalidates the other slot's header, so after `--activate` the previous slot reads `no image header`. Expected; flash it again for a second bootable slot.

### When the user says the device froze after debugging

- `HALTED` in `status` means a debugger left the cores halted, or the firmware crashed and halted itself. A `[CRASH]` report in the log means a crash; without a log, `swd where` tells: after a crash core 0 sits in `__furi_crash_implementation`.
- Cores left halted by a debugger: `swd resume`. To prevent it, the debug configurations resume the cores on detach: [docs/Debugging.md](../../docs/Debugging.md#debugger).

### When the firmware crashes or hangs

After a crash the firmware prints a report to the MCU log and stops; it does not reboot at this stage of development. The console shows no prompt, and `screenshot` and `press` fail. The report does not survive a reset and `dmesg` starts empty, so **keep `f1.py log --out FILE` running in the background while testing something that may crash**.

| What happened | MCU log | `status` |
|---|---|---|
| `furi_check`, `furi_assert`, `furi_crash` | `[CRASH][<thread>] <message>`, registers `r0`..`r11` and `lr`, stack and heap watermarks, `System halted` | `HALTED` if an SWD command ran before the crash with no probe-pin reset since; otherwise `running`, uptime still counting (see [Pitfalls](#pitfalls)) |
| HardFault | `[E][FuriHalDebug] HardFault! lr=... pc=...`, registers, `CFSR`/`HFSR`/`MMFAR`/`BFAR`, free stack per task (`*** LOW ***` marks tight ones), then `[CRASH][ISR HardFault]` | same |
| Hang (endless loop, interrupts off) | nothing | `running`, uptime counting |

- `running` only means the core executes. A dead console with `running` is a crash or a hang; only the log tells which.
- Turn addresses into source lines with `f1.py swd symbol 0x1000a133`. In a `[CRASH]` report `lr` points into the function that failed the check. In a HardFault report `pc` is where it faulted and `lr_st` the saved return address (the caller); its plain `lr` is an exception code, not an address.
- The ELF must be the running build, or the lines are wrong. The tools take the newest `.elf` in `build/` or `build/<preset>/`.
- For a hang, `f1.py swd where` shows the stuck line; after a crash it shows only the crash handler.
- Fault registers without halting: `f1.py swd read 0xE000ED28 4` gives CFSR, HFSR, DFSR, MMFAR.
- Recover with `f1.py swd reset` or a probe-pin reset. `swd resume` does not help: the firmware halts again at once.
- Linux keeps running through an MCU crash, with nothing in its kernel log, but tools needing the MCU console fail, `linux stop` included. Reset the MCU first; see [MCU reset while Linux runs](#mcu-reset-while-linux-runs).

## Pitfalls

- **MCU timestamps stop during flash erase**: each sector erase masks interrupts and OS ticks are lost. Once, 119 sectors took 5.8 s of wall time while the MCU log advanced 0.24 s.
- **I²C device errors after an MCU reset with Linux running are expected.** PCAL6416, FUSB302, BQ2579x and INA219 share a bus with the host, so their init can fail when the MCU boots under a running Linux, a different device each time or none (`Failed to read reg`, `Failed to reset FUSB302!`, `Failed to initialize INA219`, `Failed to initialize Main Expander`). Linux may log the other side (`pca953x ... failed reading register`, `SDA held low, recovering the bus`). A known architectural limitation; do not report it as a bug in your change.
- **Attaching over SWD leaves debug enabled in the core**, across `swd reset` too; only a probe-pin reset clears it. Meanwhile a breakpoint instruction halts the core instead of faulting, which is how a crashed firmware ends up `HALTED`.
- **A halted MCU stops everything it manages**: screen, buttons, reactions to the host. `swd flash` halts it for the 8 s of the write; a running Linux carries on.
- **Two tools cannot share a port**: the second fails at once with `is in use by another process`. Different ports work in parallel; see [Ports](#ports).

## What still needs the user

- You need SWD while a debug session holds the probe.
- The device needs a cable, a physical button not in the list above, or BOOTSEL on a board or probe whose firmware has no working `power boot`.
- The device was switched off with `power off` or `power ship`.
- You are about to erase or overwrite something and have not been told the device is free to use.
- The tools report the MCU running, but the screenshot or the user says otherwise.

## End of session

- Stop any background `log`.
- Tell the user the state you leave: the active slot and each slot's contents, whether Linux is up, files left on the host.

## Source layout and tests

| Path | What |
|---|---|
| `f1.py` | Entry point: builds the parser, calls one command |
| `cli/main.py` | The commands and their text output, nothing else |
| `logic/` | Device services: console, RPC session, Linux host, SWD, slot flashing |
| `transport/` | Byte and line movers: serial port, OpenOCD process, port discovery |
| `packets/` | Pure codecs: UF2, picobin, RPC protobuf subset, console text, U-Boot text, OpenOCD output |
| `openocd/flipperone-mcu.cfg` | Board config: the SDK RP2350 target plus the helpers in [Firmware slots and SWD](#firmware-slots-and-swd) |
| `tests/` | Unit tests with fake ports |

Imports run one way, `cli` → `logic` → `transport` → `packets`; `tests/test_layers.py` fails on any import pointing back up. The tests need only `scripts/requirements-test.txt`: no firmware build, device or probe. CI runs them in `.github/workflows/agent-tools-tests.yml`. Command: [docs/Testing.md](../../docs/Testing.md#host-tests).
