# Flipper One hardware tools for LLM agents

You have shell access to a development machine with a Flipper One and a Flipper One Debug Probe attached. With `scripts/agent_tools/f1.py` you can see the device screen, press its buttons, read its logs, run commands on its MCU and on its Linux host, and reflash it. This file tells you how, and where the limits are.

```shell
python3 scripts/agent_tools/f1.py <command> --help
```

Paths in this file are relative to the repository root. `f1.py` itself works from any directory; file arguments (`OUT.png`, `FILE.bin`, `--out FILE`) are relative to the directory you run it from.

Words: the **MCU** is the RP2350 that runs this firmware. The **host** is the Linux processor inside the Flipper, which the MCU powers; names such as `cpu_app_start` and `cpu_reset` mean the same processor. The computer you run commands on is **this machine**.

Every command is non-interactive, has a timeout (except `log` without `--seconds`), prints plain text, and exits non-zero with a one-line `error:` on failure. Python standard library only on macOS and Linux; Windows needs `pyserial` and is untested.

Everything below was exercised from macOS against a Flipper One rev. F0B1C2.

The debug probe needs firmware 0.7.0 or newer:

- Probe firmware is published in the releases of https://github.com/flipperdevices/flipperone-debug-probe.
- A probe on 0.7.0 or newer is updated with `f1.py probe flash FILE.uf2 --yes`.
- An older probe shows up in `python3 scripts/get_port.py` with the serial `flip_one_debug` instead of `f1_debug_xxxx_`, and every command that talks to the device is meant to fail with a message that says so. It has to be put into BOOTSEL by hand: ask the user to hold the probe's `MCU RESET & PROBE BOOT` button while plugging its USB cable in, then copy the `.uf2` onto the `RP2350` drive that appears.

## TODO

Not done or not verified yet. Treat these as unknown, not as working:

- Port lookup on Linux and Windows. It follows how those systems name CDC ports and has not been run there.
- Probe selection on Windows is not implemented: `F1_PROBE` has no effect there, set `F1_PORT_<ROLE>` instead.
- Flashing the Linux host through maskrom. Entering maskrom works; the flashing itself was not tried.
- The `Maskrom` item in the MCU Settings menu.
- The refusal of a probe with firmware older than 0.7.0 was not exercised: no such probe was left to try.

## Start of session

```shell
python3 scripts/agent_tools/f1.py status
```

`status` shows the probe and its firmware version, which ports are connected, whether the MCU is running or halted, which firmware slot is active and what is in each slot. For the state of Linux run `f1.py linux state`.

## Commands

| Command | What it does |
|---|---|
| `status` | Probe firmware, ports, MCU state with its uptime, firmware slots. `--no-swd` leaves out MCU state and slots, which are read over SWD; without it and with the probe held by a debugger, the serial part is printed and the command then fails. Its `linux` line is only a guess from the `mcu-usb` port; use `linux state` |
| `ports` | Serial ports by role |
| `screenshot OUT.png` | Device screen, 258x144 grayscale, saved 3x enlarged |
| `press BUTTON...` | Click buttons in order, 0.15 s apart. `--shot OUT.png` takes a screenshot afterwards in the same session |
| `press BUTTON --hold S --repeats N` | Long press: `press`, `long` after `S` seconds, then `N` `repeat` events 50 ms apart (default 0), then `release`. With several buttons each is held in turn; two buttons cannot be down at once |
| `press BUTTON --action press` | One raw event (`press`, `release`, `short`, `long`, `repeat`) |
| `touch X Y [--to X Y]` | Tap or swipe on the touchpad: x 0..1023 left to right, y 0..767 top to bottom, `--pressure` 0..16383 (default 8000). A swipe is 10 moves 20 ms apart; `--steps` changes the count. Takes `--shot OUT.png` like `press` |
| `cli "CMD"...` | MCU console commands, e.g. `cli uptime "power info"` |
| `probe "CMD"...` | Console commands of the debug probe itself, e.g. `probe device_info` |
| `probe flash FILE.uf2 --yes` | Reflash the debug probe and wait until it is back |
| `log --seconds N` | MCU log with this machine's time on every line. `--out FILE` appends to a file, flushed line by line; without `--seconds` it runs until killed, and killing it is the way to stop it |
| `linux state` / `start` / `stop [--hard]` | Query, boot or shut down the Linux host |
| `host "CMD"` | One shell command line on the Linux host over its serial console (join several with `;` or `&&`; nothing interactive); prints output and exit code. `--wait S` first waits up to `S` seconds for a shell prompt, for a host that is still booting |
| `swd status` / `reset` / `resume` / `read ADDR [N]` | SWD through OpenOCD, without halting unless stated. `read` prints `N` 32-bit words |
| `swd where` | Halt for a moment and print where both cores are (function, file, line), then run on |
| `swd symbol ADDR...` | Function, file and line for addresses from a crash log; uses `build/*.elf`, no probe needed |
| `swd flash a\|b FILE.bin --yes` | Write a firmware image to a slot, verify, reset |
| `swd erase-header a\|b --yes` | Make a slot unbootable, so that the other one boots, and reset. Only flashing the slot again brings it back |

Buttons: `ok back up down left right key1 key2 key4 key5 power sw ptt`. `power` is the key that would be `key3`. What a button does depends on the app on screen; `sw`, for one, opens the app switcher in Linux.

Timeouts: `cli` and `probe` wait 5 seconds per command, `host` 30, `linux start` 120, `linux stop` 90. `--timeout SECONDS` changes it for these commands, which a long `host` command needs. Under `cli` and `probe` it applies to each command separately; under `host` it counts from the moment the command starts, after any `--wait`, and a command that ran out of time keeps running until the next `host` call interrupts it. The RPC commands (`screenshot`, `press`, `touch`) give up after a few seconds, `swd` commands after 2 minutes (`swd flash` 5).

Exit codes: 0 on success, 1 on a tool error. `host` exits with the exit code of the command it ran and prints `[exit code N]` on stdout when that is not 0; a failure of the tool itself prints a line starting with `error:` on stderr instead, which is how to tell a tool error from a command that returned 1.

Commands that need `--yes` refuse without it and change nothing. Write addresses with `0x`.

## Rules

- **An unchanged screenshot is not proof that a press was ignored.** Some actions take seconds to show. Check another signal (`linux state`, `log`, `host`) before repeating a press that powers something off, confirms, or deletes.
- **Use the cheapest signal that answers the question.** `linux state`, `cli "desktop get_run_app"`, `status` and `log` are text and cost little; a screenshot costs an image. Take one when the answer is on the screen and nowhere else: before you press buttons on a screen you have not seen, and when the screen itself is what you are checking.
- **Use these tools for SWD, never `probe-rs`.** One-shot `probe-rs` reads left the device frozen until a debugger attach and detach. The cause is unknown.
- **The probe accepts one SWD client.** Only the `swd` commands and `status` without `--no-swd` use SWD; everything else goes over serial ports and works during a debug session. While a debugger holds the probe, an SWD command fails within a few seconds with `The probe is probably held by another debugger` (OpenOCD: `CMSIS-DAP command CMD_INFO failed`). That is not a fault: ask the user, or do without SWD.
- **Run `host sync` before anything that cuts host power**: `cli "desktop stop_app"` while the host app runs, `Stop` or `Reboot` in the MCU power menu, `cli "power off"` / `cli "power ship"` / `cli "power reboot"`. `linux stop --hard` runs it for you; it is `desktop stop_app` with a `sync` in front. A soft reset of the MCU (`swd reset`, `swd flash`) does not cut it, but see [MCU reset while Linux runs](#mcu-reset-while-linux-runs).
- **Check each step before the next.** Read state back after every action and stop if it is not what you expected.
- **Ask before destructive actions** unless the user said the device is free to use: `swd flash`, `swd erase-header`, `probe flash`, `linux stop --hard`, whatever else cuts host power under a running Linux (the list in the rule above), and what switches the device off or sends the MCU to its bootloader: `cli "power off"`, `cli "power ship"`, `cli "power boot"`, `Power Off` in the MCU power menu. `--yes` is how you tell the tool that they agreed. No need to ask before `linux stop`, `swd reset`, a reset of the MCU through the probe, `probe "power reboot"`, or starting and leaving maskrom mode with the host off.
- **Measure time from `log` timestamps**, which are this machine's time. The MCU's own timestamps lose time (see [Pitfalls](#pitfalls)).
- **If what you observe contradicts what the user says about the device, the user is right.** They are looking at it.
- **Report what you verified and what you did not**, and the state you leave the device in.

## Seeing and pressing

The screen and input go through the MCU's RPC protocol (`assets/proto/rpc.proto`), reached with the `rpc` command of the MCU console. Each `screenshot`, `press` or `touch` opens a session and closes it again.

- The screen is whatever the device display shows: the MCU desktop, the host's boot menu, or the host's panel UI once Linux is up.
- A screenshot takes about a second. Starting the capture makes the MCU redraw, so you get the current screen even when nothing on it is changing.
- A click sends `press`, `short`, `release`, the order the firmware itself uses. For a physical button the firmware produces `short`, `long` and `repeat` itself from how long it is held; for injected events it does not, so the tool sends each of them. Which event an app reacts to is up to the app; most of the MCU UI reacts to `press`. Firmware older than the `SHORT`/`LONG`/`REPEAT` protocol extension logs `Unknown action` for `short` and ignores it; clicks still work there because the UI reacts to `press`.
- Menus scroll on `press` and `repeat`. To move several items, send the button several times: `press down down down`.
- Some screens want a button held. The Keypad Test (Settings, Testing, Keypad) ignores a short Back and exits when Back is held for nine repeat events: `press back --hold 0.5 --repeats 9`. `cli "desktop stop_app"` closes any MCU app without buttons. Check `cli "desktop get_run_app"` first: if the app is `cpu_app_start`, closing it cuts host power.

Touch events are delivered as is, like button events. What a point means on screen is up to the app: the Touchpad Test, for example, maps only the central 70% by 60% of the range onto its drawing area.

Only one MCU app runs at a time; the desktop itself is not an app. `cli "desktop get_run_app"` tells which (`no app is running` on the desktop), `cli "desktop start_app <appid>"` and `cli "desktop stop_app"` switch; `cli "desktop list_apps"` lists them.

On the MCU desktop: `key2` is Help, `power` opens the power menu, `key4` is Settings. The power menu depends on the host. While Linux is off it has `Start Flipper OS` and `Power Off`; `Power Off` switches the whole device off (ship mode), and only the user can bring it back, by holding the power button. While Linux runs it has `Stop` and `Reboot`, which act on the host, not on the MCU. The `power` button always belongs to the MCU, also while Linux owns the rest of the screen and buttons. In menus `up`/`down` move, `ok` selects, `back` leaves.

## Starting and stopping Linux

Linux does not start by itself after the MCU boots, after a shutdown, or after a host `reboot` (it then waits in the boot menu). Control it with:

| Command | What it does | Typical time |
|---|---|---|
| `linux state` | One of: `off`; `powered, no shell prompt`; `up: shell answers`; `up: shell answers, but the MCU host app is not running`; `powered in maskrom mode` | 1 s |
| `linux start [--entry N\|NAME]` | Powers the host, lists the boot entries, picks one (default 1), waits for the shell | 30 to 45 s |
| `linux stop` | `sync`, then `poweroff` inside Linux; waits until the MCU reports the host off | 7 to 20 s |
| `linux stop --hard` | Tries `sync`, then the MCU cuts host power. Linux gets no warning | 3 s |

- Prefer `linux stop`. Use `--hard` when the test is about power loss or the host no longer answers.
- None of this needs a screenshot: every step is confirmed from the consoles.
- `linux start` fails if another MCU app is running. The usual one is the self-check (`self_check`), which runs at every MCU boot and stays on screen when one of its checks failed, a device not found for instance; `press back` or `press ok` closes it, and `press back` does nothing on the desktop. The first time it stays open in a session, take a screenshot, close it, carry on, and mention it to the user in your next message; after that do as they say.
- `linux start` prints the boot entries it found, numbered as in the on-screen menu, for example `boot entry 7: Minimal-7.4.0-rc1-g6cd481aae87d`. `--entry` takes that number, or text that exactly one entry name contains, in any case: `--entry router-7`. Text that matches no entry or several fails with the list printed and the host powered off again, which is also the way to see the entries of an unknown image.
- `powered, no shell prompt` covers a host that is still booting, one that waits in the boot menu (after a host `reboot`, for instance), one at the U-Boot prompt, and one at a login prompt. Give a booting host time with `host "true" --wait 60`. In the boot menu pick an entry with the buttons: `press down ... ok`. `linux start` only prints `host is already powered` while the MCU host app is running; if the host stays without a prompt, `linux stop --hard` and `linux start` is the way out.
- `linux stop` also works on a Linux that runs without the MCU host app: it takes the kernel's `Power down` line as the end.
- If `linux start` reports no sign of life from the host, the MCU has lost track of host power: reset the MCU (`swd reset`, then `press back` if the self-check stays open) and start again.
- An entry can be present and still not boot. `linux start` then fails with U-Boot's message and leaves the host at the U-Boot prompt; `linux stop --hard` gets out of it.

How it works, for when you need the pieces:

- The MCU powers the host while its app `cpu_app_start` runs: `cli "desktop start_app cpu_app_start"` starts it, `cli "desktop stop_app"` cuts power, `cli "desktop get_run_app"` tells whether it runs. `cli "desktop list_apps"` shows the other MCU apps, including `cpu_app_maskrom`.
- U-Boot scans for images for several seconds, prints `(N bootflows, N valid)` on the Linux console, then shows a menu on the device screen and waits; it did not time out in 70 seconds. The scan output on the Linux console lists the entries (`/boot/loader/entries/*.conf`) in menu order. It takes keys from the Linux console (arrow keys and Enter) and from the device buttons.
- When Linux powers itself off, the MCU app exits by itself.
- `linux stop` waits for the kernel's `reboot: Power down` line and types nothing meanwhile. Do the same if you shut down by hand: a Ctrl+C on the Linux console interrupts the pending `poweroff`.
- systemd ignores a `poweroff` issued while the system is still starting up, which lasts some tens of seconds after the shell first answers. `linux stop` sends it again.

### Maskrom mode

`cli "desktop start_app cpu_app_maskrom"` powers the host with its maskrom line held for one second, so the SoC stays in its boot ROM and waits for a loader over its own USB port instead of booting. `cli "desktop stop_app"` powers the host off again; a normal `linux start` works afterwards.

- What you can see of it: the MCU log has `CPU app started with arg: maskrom`, the Linux console stays silent (a normal start prints DDR training and U-Boot within a second), the device screen is black, `linux state` says `powered in maskrom mode`.
- The MCU Settings menu has a `Maskrom` item as well; it was not tried.
- Flashing the host through maskrom needs a USB cable from the host's port to this machine and Rockchip tooling. That part was not tried here.

### MCU reset while Linux runs

A reset of the MCU alone (`swd reset`, `swd flash`, the probe's `mcu_reset`, a firmware crash) leaves Linux running: the host power rails are held by an I/O expander that keeps its outputs. Afterwards:

- The MCU is back on its own desktop with no host app running, so the device screen and buttons belong to the MCU while Linux runs unseen. `linux state` reports exactly this.
- `linux start` attaches to the running Linux: the host app sees on the expander that the host is powered and leaves it alone, the screen shows Linux again, nothing restarts, and `--entry` is ignored. The tool runs `sync` on the host first and checks afterwards that the host uptime went on.
- Rarely the MCU fails to initialise that expander when it boots, because the bus is shared with the running host (see [Pitfalls](#pitfalls)); the MCU log then has `Failed to initialize Main Expander` and the MCU's `cli dmesg` has `Not initialized Main Expander`. Until the next reset the MCU has no control over host power. `linux start` checks that and refuses; reset the MCU again and retry. Linux itself is not disturbed by that state.
- Should the host restart anyway when the host app starts, `linux start` notices by the host uptime and says so.
- By hand it is the same: `Start Flipper OS` in the power menu, or `cli "desktop start_app cpu_app_start"`.

### By hand, through the screen

The same can be done as a person would. Use it only when the screen, the buttons or the UI/UX are what you are testing; it needs screenshots and the commands above do not.

- Start: `press power --shot s.png` shows `Start Flipper OS` selected, `press ok`; after about 10 seconds a screenshot shows the U-Boot menu; `press down ...` and `press ok`.
- Clean stop: on the host panel `press key2` (Menu), select `Settings`, scroll to `Shutdown` (the last item, after `Reboot`), `press ok`. No confirmation dialog. The screen keeps showing the list, then goes black for several seconds; neither means the press failed.
- Hard stop: with Linux running `press power` shows `Flipper OS` with `Stop` and `Reboot`; `press ok` on `Stop` cuts power within a second.

### Hard stop side effects

The MCU log shows `Bq2579x` errors (`Failed to write reg 0x01`, `Failed to set charge voltage limit!`) at the moment of a hard stop. The stop path resets the charger and USB PD controller; the errors look like the shared I2C bus described under [Pitfalls](#pitfalls), though that cause is not confirmed. They do not appear on every hard stop.

## Ports

| Role | What | Baud |
|---|---|---|
| `mcu` | MCU console through the probe's UART, always available; the tools use this one | 1500000 |
| `mcu-usb` | Same console over the MCU's own USB; needs a cable from the Flipper's USB port to this machine and exists only while the host is off, because the Flipper's USB port is switched over to the host when it is powered | any |
| `mcu-log` | MCU log, output only | 1500000 |
| `linux` | Linux serial console | 1500000 |
| `probe` | Console of the debug probe itself | any |

- Tools that need the MCU console use `mcu`. `--console mcu-usb` (accepted by `cli`, `screenshot`, `press` and `touch`) selects the USB one, but it vanishes mid-command when Linux powers up.
- The probe runs its UART bridges at a fixed 1500000 baud whatever rate this machine asks for, unless custom baud rates are enabled on the probe (`f1.py probe "settings set_custom_baudrate 1"`).
- A present `mcu-usb` port means the host is off. An absent one means nothing: there may simply be no cable. Use `linux state`.
- Port lookup lives in `scripts/get_port.py`, which also works on its own: `python3 scripts/get_port.py mcu` prints the device path, without arguments it lists all roles.
- Ports are found by USB identity and CDC interface: probe `37c1:f131` serial `f1_debug_xxxx_`, control interface 1 for `linux`, 3 for `mcu`, 5 for `mcu-log`, 7 for `probe`; MCU `37c1:f101` serial `flip_one_mcu`, interface 0. macOS names a port after the data interface, which is the next number: `..._2` is `linux`, `..._4` is `mcu`, `..._6` is `mcu-log`, `..._8` is `probe`. Override a port with `F1_PORT_<ROLE>`, for example `F1_PORT_MCU_LOG=/dev/ttyACM2`.
- Which command holds which port: `cli`, `screenshot`, `press`, `touch` use `mcu`; `log` uses `mcu-log`; `host` uses `linux`; `linux` uses `mcu` and `linux`; `probe` uses `probe`; `status` uses `probe` and SWD; `swd` uses SWD only. So a background `log` gets in the way of nothing but another `log`.
- The UART consoles (`mcu`, `linux`) have no notion of a client connecting, so they print nothing when a port is opened; a prompt appears only after you send something. The tools send Ctrl+C and Enter first.

### Several probes

Each probe has its own USB serial number, `f1_debug_xxxx_`, where the four hex digits are folded from its chip ID. The full chip ID is at the end of the USB product string, for example `Flipper One Debug Probe 63E7482F2DB3DF2B`. On macOS the serial is part of the port name: `/dev/cu.usbmodemf1_debug_d950_4`.

- `ports` lists the attached probes with their IDs and serial numbers.
- With one probe attached nothing needs to be set. With several, every command stops and lists them until you choose: `F1_PROBE=d950 python3 scripts/agent_tools/f1.py status`. `F1_PROBE` takes the four digits of the serial, or the chip ID or its beginning.
- The choice applies to everything: serial ports and SWD. For SWD the tools pass `adapter serial f1_debug_xxxx_` to OpenOCD.
- The `mcu-usb` port is not tied to a probe. With more than one board on USB it is left out; set `F1_PORT_MCU_USB` if you need it.
- If a cable may have moved, confirm the board behind the probe: `cli device_info` prints the MCU's hardware UID, and the Linux prompt shows the hostname.

### The probe's own console

The `probe` port is the console of the debug probe itself; `f1.py probe "CMD"...` runs commands there.

| Command | Use |
|---|---|
| `device_info` | Probe firmware version, commit and chip ID, in the same format as the MCU's `device_info` |
| `gpio mcu_reset 0`, then `gpio mcu_reset 1` | Hold the Flipper's MCU in reset, then release it |
| `power boot` | Reboot the probe into its USB bootloader: a drive named `RP2350` appears within a few seconds, copying a `.uf2` onto it flashes the probe. `f1.py probe flash FILE.uf2 --yes` does all of that |
| `power reboot` | Restart the probe |
| `help` | All commands |

- Resetting the MCU through the probe does not use SWD, so it works while a debugger holds the probe: `f1.py probe "gpio mcu_reset 0" "gpio mcu_reset 1"`. For Linux it is the same as `swd reset`: the host keeps running, see [MCU reset while Linux runs](#mcu-reset-while-linux-runs).
- `power boot` and `power reboot` have the same names as on the MCU. Neither disturbs the Flipper's MCU.
- `gpio` also lists `cpu_reset` and a few general-purpose debug pins. The purpose of those pins changes between firmware versions; do not rely on them.

## MCU console

| Command | Use |
|---|---|
| `uptime` | Seconds since MCU boot; a quick "did it reset?" check |
| `device_info` | Firmware branch, commit and version actually running |
| `dmesg` | The last 10 KB of the MCU log, kept in RAM. Recording starts when the MCU apps start (first line around `Starting self check autorun`), so the boot lines before that are never in it, and older lines are overwritten. Use it to look back when `f1.py log` was not running; use `f1.py log` to capture everything from now on |
| `power info` | Battery, USB and charger readings, once |
| `power show` | The same in full detail, repeated until interrupted: under `cli` it ends in a timeout error with the readings printed |
| `help` | All commands |

Do not run `power off`, `power ship`, `power reboot` or `power boot` unless that is the point of the test. `power reboot` cuts power to the whole device, the host included, and the device comes back by itself within seconds. `power off` and `power ship` both switch the whole device off: after `power ship` it comes back when the user holds the power button, after `power off` only when USB power is connected. `power boot` resets the charger and the USB PD controller and reboots the MCU into its USB bootloader (BOOTSEL): the firmware stops, the MCU console goes silent, `status` shows `active none`, and with a cable on the Flipper's USB port a drive named `RP2350` appears on this machine. `swd reset` or a reset through the probe brings the firmware back. A running Linux stays up through `power boot`.

If the MCU console was left in a bad state (a command still running, or a crashed RPC session), the tools recover it on the next call. If they report no prompt, the MCU itself is down: see [When the firmware crashes or hangs](#when-the-firmware-crashes-or-hangs).

## Linux host

- `host "CMD"` works without network. The console is already logged in as `user`; `sudo` needs no password. If the console sits at a login prompt instead, `host` fails with `shows no shell prompt`; logging in is not automated.
- The image build is in `/etc/os-release` (`BUILD_GIT`, `BUILD_ID`), the kernel in `uname -r`.
- For file transfer use SSH: `user@<address>`, password `user`. `host "hostname -I"` prints the address, or nothing when the host has no network.

## Firmware slots and SWD

SWD goes through OpenOCD from the Pico SDK (`~/.pico-sdk/openocd/<version>/openocd`, or the binary named in `F1_OPENOCD`), which this repository needs anyway; a stock OpenOCD without RP2350 support will not do. To call it yourself: `-f interface/cmsis-dap.cfg -c "adapter serial f1_debug_xxxx_" -f target/rp2350.cfg -c "adapter speed 5000"`. `swd symbol` uses `arm-none-eabi-addr2line` from the Pico SDK toolchain.

- There are two firmware slots, A and B. The bootrom boots the valid image with the higher version; with equal versions it boots A.
- `build/flipperone-mcu-firmware.bin` runs from either slot. `swd flash` wants the `.bin`.
- Flash your builds to slot A. Local builds all carry the same version, and with equal versions A wins, so a build written to B does not start while A holds a valid image of the same or a higher version. If B holds a higher version than your build, write the build to both slots, or make B unbootable with `swd erase-header b`.
- `status` reports per slot: `erased`, `no image header`, or `valid` with its version words. The first word packs `FW_IMAGE_MAJOR` (bits 15..12), `FW_IMAGE_MINOR` (11..4) and `FW_IMAGE_PATCH` (3..0) from `CMakeLists.txt`, so 0.1.0 is `0x0010`. The second word is reserved for a boot counter and is 0 in every build today. `DIST_SUFFIX` changes only the version string that `device_info` prints, not these words.
- After flashing, confirm with `status` that the slot you wrote is the active one, and with `cli device_info` that the running build is yours. An image with a lower version than the other slot will not be chosen.
- `active none` means the core sits in the bootrom: either no slot holds a bootable image (flash slot A over SWD to recover), or the MCU was sent to its USB bootloader with `power boot` (reset it).
- `swd read` addresses worth knowing: `0x400d0034` active slot (low 12 bits = the slot's offset divided by `0x1000`: `0x002` for A, `0x782` for B, 0 when nothing booted), `0x400b0028` microseconds since MCU boot, `0xE000EDF0` debug status (bit 17 = halted), `0x1c000000 + offset` raw flash.

### Build, flash, check

```shell
DIST_SUFFIX=my-test-1 cmake -S . -B build && cmake --build build   # own version string, to tell builds apart
python3 scripts/agent_tools/f1.py swd flash a build/flipperone-mcu-firmware.bin --yes
python3 scripts/agent_tools/f1.py status              # slot A is the active one
python3 scripts/agent_tools/f1.py cli device_info     # the running build is yours
python3 scripts/agent_tools/f1.py press back          # close the self-check, if the MCU opened it after booting
```

- `swd flash` takes about 8 seconds. The MCU console answers within a second after `swd flash`, `swd reset` and a reset through the probe; no waiting is needed before the next command.
- With Linux running, flashing resets only the MCU; run `linux start` afterwards to attach to it again, see [MCU reset while Linux runs](#mcu-reset-while-linux-runs).

### When the user says the device froze after debugging

- `status` showing `HALTED` has two causes: a debugger detached and left the cores halted, or the firmware crashed and halted itself (see [When the firmware crashes or hangs](#when-the-firmware-crashes-or-hangs)). A `[CRASH]` report in the MCU log means the second. Without a log, `swd where` tells: after a crash core 0 sits in `__furi_crash_implementation`.
- Cores left halted by a debugger are fixed with `swd resume`.
- For a cortex-debug `launch.json`, these lines in `openOCDLaunchCommands` make the debugger resume the cores when it detaches:

```json
"rp2350.dap.core0 configure -event gdb-detach { resume }",
"rp2350.dap.core1 configure -event gdb-detach { resume }"
```

### When the firmware crashes or hangs

The firmware does not reboot after a crash at this stage of development: it prints a report to the MCU log and stops. The MCU console then shows no prompt, and `screenshot` and `press` fail with it. Nothing of the report survives a reset, and `dmesg` starts empty again, so **keep `f1.py log --out FILE` running in the background while you test something that may crash**; afterwards there is nothing to read.

| What happened | MCU log | `status` |
|---|---|---|
| `furi_check`, `furi_assert`, `furi_crash` | `[CRASH][<thread>] <message>`, registers `r0`..`r11` and `lr`, stack and heap watermarks, `System halted` | `HALTED` if an SWD command had been run before the crash and no reset through the probe pin came in between; otherwise `running` with the uptime still counting, however often you ask afterwards (see [Pitfalls](#pitfalls)) |
| HardFault | `[E][FuriHalDebug] HardFault! lr=... pc=...`, registers, `CFSR`/`HFSR`/`MMFAR`/`BFAR`, free stack of every task (`*** LOW ***` marks the tight ones), then `[CRASH][ISR HardFault]` | same |
| Hang (endless loop, interrupts off) | nothing | `running`, and the uptime in `status` keeps counting |

- `running` in `status` only means the core executes instructions. A dead console with `running` is a crash or a hang, and `status` cannot tell them apart; the log does.
- Turn addresses from the report into source lines: `f1.py swd symbol 0x1000a133`. In a `[CRASH]` report `lr` points into the function that failed the check. A HardFault report has `pc`, where it faulted, and `lr_st`, the return address saved at that moment, which is the caller; its plain `lr` is an exception code, not an address.
- For a hang, `f1.py swd where` shows the line it is stuck on. After a crash it only shows the crash handler; use the log.
- The ELF in `build/` has to be the build that is running, otherwise the lines are wrong. Keep one `.elf` there: the tools take the first one they find.
- Fault registers can also be read without halting: `f1.py swd read 0xE000ED28 4` gives CFSR, HFSR, DFSR, MMFAR.
- To get going again: `f1.py swd reset`, or the reset pin through the probe. `swd resume` does not help after a crash, the firmware halts again at once.
- Linux keeps running when the MCU crashes, with nothing about it in the kernel log. The tools that need the MCU console fail meanwhile, `linux stop` among them. Reset the MCU first; `linux start` then attaches to the running Linux, see [MCU reset while Linux runs](#mcu-reset-while-linux-runs).

## Pitfalls

- **MCU timestamps stop during flash erase.** Each sector erase masks interrupts and OS ticks are lost. In one measurement the firmware erased 119 sectors in 5.8 s of wall time while the MCU log advanced 0.24 s.
- **I2C device errors after an MCU reset with Linux running are expected.** PCAL6416, FUSB302, BQ2579x and INA219 sit on a bus shared with the host. If the MCU boots while Linux is up, their init can fail, a different device each time or none (`Failed to read reg`, `Failed to reset FUSB302!`, `Failed to initialize INA219`, `Failed to initialize Main Expander`). Linux can show the other side of it in its kernel log around an MCU reset (`pca953x ... failed reading register`, `SDA held low, recovering the bus`). This is a known architectural limitation; do not report it as a bug in the change you are testing.
- **Attaching over SWD leaves debug enabled in the core**, also across `swd reset`; a reset through the probe pin clears it. While it is enabled, a breakpoint instruction in the firmware halts the core instead of faulting, which is how a crashed firmware ends up `HALTED`.
- **A halted MCU stops everything it manages**: the screen, the buttons, reactions to the host. `swd flash` halts it for the 8 seconds of the write; a running Linux carries on through that.
- **Two tools cannot share a port.** A command that finds its port taken fails at once with `is in use by another process`. Different ports are fine in parallel; see [Ports](#ports) for which command uses which.

## What still needs the user

- You need SWD and a debug session holds the probe.
- The device needs a cable, a physical button that is not in the list above, or BOOTSEL on a board or probe whose firmware has no working `power boot`.
- The device was switched off with `power off` or `power ship`: only the user can bring it back.
- You are about to erase or overwrite something and have not been told the device is free to use.
- The tools report the MCU as running but the screenshot or the user says otherwise.

## End of session

- Stop any background `log`.
- Tell the user the state you leave: active slot and what is in each, whether Linux is up, files you left on the host.
