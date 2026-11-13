#!/usr/bin/env python3
"""Tools for an LLM agent to see and drive a Flipper One on the bench.

Every command is non-interactive, has a timeout, prints plain text and exits
non-zero with a one-line reason on failure. See AGENT_TOOLS.md next to this file.
"""

import argparse
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(1, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))  # scripts/, for get_port

import mcu  # noqa: E402
import get_port as ports  # noqa: E402
import swd  # noqa: E402
from serialport import SerialPort  # noqa: E402


def open_role(role):
    return SerialPort(ports.require(role), ports.baud(role))


def open_mcu_console(args=None):
    role = getattr(args, "console", None) or ports.mcu_console_role()
    return open_role(role)


def report_port_lost(rpc):
    if rpc.port_lost:
        print("note: the MCU USB console disappeared during the command (expected when Linux powers up); use the default UART console")


# --- commands ---------------------------------------------------------------


def cmd_ports(args):
    attached = ports.probes()
    for probe in attached:
        print(f"probe {probe['id']}  serial {probe['serial']}")
    if len(attached) > 1 and not os.environ.get("F1_PROBE"):
        print("several probes attached: set F1_PROBE=<ID or its beginning> to choose one")
        return
    for role, (_, _, baud, description) in ports.ROLES.items():
        print(f"{role:8} {ports.find(role) or '-':44} {baud:>8}  {description}")
    if ports.find("mcu-usb"):
        print("mcu-usb is present: Linux is not running")


def probe_firmware():
    """One line about the selected probe's firmware, from its own console."""
    probe = ports.selected_probe()
    if not probe:
        return "not connected"
    try:
        with open_role("probe") as port:
            console = mcu.McuConsole(port)
            console.sync()
            info = console.command("device_info")
    except (OSError, TimeoutError, RuntimeError) as error:
        return f"{probe['id']}, console did not answer ({error})"
    fields = dict(re.findall(r"firmware_(version|commit|commit_dirty)\s*:\s*(\S+)", info))
    return f"{probe['id']}, firmware {fields.get('version', '?')} commit {fields.get('commit', '?')}" + (" dirty" if fields.get("commit_dirty") == "true" else "")


def cmd_status(args):
    print("probe    " + probe_firmware())
    for role in ports.ROLES:
        print(f"port {role:8} {ports.find(role) or 'not connected'}")
    print("linux    " + ("not running (mcu-usb port is present)" if ports.find("mcu-usb") else "may be running (no mcu-usb port)"))
    if args.no_swd:
        return
    state = swd.status()
    print("mcu      " + ("HALTED: by a debugger, or by the firmware after a crash (see the log)" if state["halted"] else "running") + f", uptime {state['uptime_s']:.1f}s")
    print("active   " + (f"slot {state['active_slot']}" if state["active_slot"] else "none: nothing booted, core is in the bootrom"))
    for name, slot in state["slots"].items():
        line = f"slot {name}   {slot['offset']:#08x} {slot['state']}"
        if slot["state"] == "valid":
            line += f", version words {slot.get('version_major_word', 0):#06x}/{slot.get('version_minor_word', 0)}"
            if slot.get("tbyb_pending"):
                line += ", try-before-you-buy not bought (ignored on a normal boot)"
        print(line)


def cmd_cli(args):
    with open_mcu_console(args) as port:
        console = mcu.McuConsole(port)
        console.sync()
        for line in args.command:
            if len(args.command) > 1:
                print(f"> {line}")
            print(console.command(line, args.timeout))


def bootloader_drive():
    """Mount point of an RP2350 USB bootloader drive, or None."""
    user = os.environ.get("USER", "")
    for path in ("/Volumes/RP2350", f"/media/{user}/RP2350", f"/run/media/{user}/RP2350"):
        if os.path.isfile(os.path.join(path, "INFO_UF2.TXT")):
            return path
    return None


def wait_for(condition, timeout, what):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        value = condition()
        if value:
            return value
        time.sleep(0.5)
    raise RuntimeError(f"{what} within {timeout:.0f}s")


def probe_flash(args, path):
    """Send the probe to its USB bootloader, copy a .uf2 onto it, wait for the probe to return."""
    if not path.endswith(".uf2") or not os.path.isfile(path):
        raise RuntimeError(f"'{path}' is not a .uf2 file")
    if not args.yes:
        raise RuntimeError("this reflashes the debug probe; repeat with --yes")
    if bootloader_drive():
        raise RuntimeError("an RP2350 bootloader drive is already mounted, so the probe's own could not be told from it; flash or eject that one first")
    probe = ports.selected_probe()
    if not probe:
        raise RuntimeError("no debug probe is attached")
    with open_role("probe") as port:
        console = mcu.McuConsole(port)
        console.sync()
        try:
            reply = console.command("power boot", 3.0)
        except (OSError, TimeoutError):
            reply = ""
    drive = wait_for(bootloader_drive, 15.0, "the probe's bootloader drive did not appear")
    with open(path, "rb") as source:
        image = source.read()
    try:
        with open(os.path.join(drive, os.path.basename(path)), "wb") as target:
            target.write(image)
    except OSError:
        pass  # the drive disappears as soon as the last block is written
    wait_for(lambda: any(p["id"] == probe["id"] and p["ports"] for p in ports.probes()), 20.0, f"probe {probe['id']} did not come back")
    time.sleep(1.0)
    with open_role("probe") as port:
        console = mcu.McuConsole(port)
        console.sync()
        info = console.command("device_info")
    version = re.search(r"firmware_version\s*:\s*(\S+)", info)
    commit = re.search(r"firmware_commit\s*:\s*(\S+)", info)
    print(f"probe {probe['id']} flashed and back" + (f": version {version.group(1)}, commit {commit.group(1) if commit else '?'}" if version else ""))


def cmd_probe(args):
    """Commands on the console of the debug probe itself."""
    if args.command[0] == "flash":
        if len(args.command) != 2:
            raise RuntimeError("usage: f1.py probe flash FILE.uf2 --yes")
        return probe_flash(args, args.command[1])
    with open_role("probe") as port:
        console = mcu.McuConsole(port)
        console.sync()
        for line in args.command:
            if len(args.command) > 1:
                print(f"> {line}")
            leaving = line.split() in (["power", "boot"], ["power", "reboot"])
            try:
                reply = console.command(line, args.timeout)
            except (OSError, TimeoutError):
                if not leaving:
                    raise
                reply = ""
            if leaving:
                # the probe leaves USB, and its ports with it; whether the reply arrives first varies
                print("the probe is restarting" if "reboot" in line else "the probe is in its USB bootloader: copy a .uf2 onto the RP2350 drive")
                return
            print(reply)


def cmd_screenshot(args):
    with open_mcu_console(args) as port, mcu.RpcSession(port) as rpc:
        width, height, pixels = rpc.screenshot()
    mcu.write_png(args.out, width, height, pixels, args.scale)
    print(f"{args.out}: {width}x{height} screen, saved at {args.scale}x")


def cmd_press(args):
    with open_mcu_console(args) as port, mcu.RpcSession(port) as rpc:
        for button in args.button:
            if args.action:
                rpc.button(button, args.action)
            else:
                rpc.click(button, hold=args.hold, repeats=args.repeats)
            time.sleep(args.delay)
        if args.shot:
            time.sleep(args.settle)
            width, height, pixels = rpc.screenshot()
    print("sent: " + " ".join(args.button))
    report_port_lost(rpc)
    if args.shot:
        mcu.write_png(args.shot, width, height, pixels, args.scale)
        print(f"{args.shot}: {width}x{height} screen, saved at {args.scale}x")


def cmd_touch(args):
    x0, y0 = args.x, args.y
    x1, y1 = args.to if args.to else (x0, y0)
    for x, y in ((x0, y0), (x1, y1)):
        if not (0 <= x <= mcu.TOUCH_MAX_X and 0 <= y <= mcu.TOUCH_MAX_Y):
            raise ValueError(f"Touch point {x},{y} is outside 0..{mcu.TOUCH_MAX_X} x 0..{mcu.TOUCH_MAX_Y}")
    with open_mcu_console(args) as port, mcu.RpcSession(port) as rpc:
        rpc.touch("start", x0, y0, args.pressure)
        steps = args.steps if args.to else 0
        for step in range(1, steps + 1):
            time.sleep(0.02)
            rpc.touch("move", x0 + (x1 - x0) * step // steps, y0 + (y1 - y0) * step // steps, args.pressure)
        time.sleep(0.05)
        rpc.touch("end", x1, y1, 0)
        if args.shot:
            time.sleep(args.settle)
            width, height, pixels = rpc.screenshot()
    print(f"touch {x0},{y0}" + (f" -> {x1},{y1}" if args.to else ""))
    if args.shot:
        mcu.write_png(args.shot, width, height, pixels, args.scale)
        print(f"{args.shot}: {width}x{height} screen, saved at {args.scale}x")


def cmd_log(args):
    """Print MCU log lines prefixed with the host time. Runs until --seconds elapse or it is killed."""
    out = open(args.out, "ab") if args.out else None
    deadline = time.monotonic() + args.seconds if args.seconds else None
    buffer = b""
    try:
        with open_role("mcu-log") as port:
            while deadline is None or time.monotonic() < deadline:
                buffer += port.read(0.5)
                while b"\n" in buffer:
                    raw, buffer = buffer.split(b"\n", 1)
                    line = f"{time.time():.2f} {mcu.clean(raw)}"
                    if out:
                        out.write(line.encode() + b"\n")
                        out.flush()
                    else:
                        print(line, flush=True)
    except KeyboardInterrupt:
        pass
    finally:
        if out:
            out.close()


CPU_APP = "cpu_app_start"  # the MCU app that powers the host and shows its screen
SHELL_PROMPT = re.compile(rb"[$#] ?$")


def host_shell_ready(port):
    """(shell prompt seen, any output seen) on the Linux console."""
    any_output = False
    for _ in range(3):  # the first Enter can land in the middle of other output
        port.read_until_quiet(0.3, quiet=0.2, wait_for_data=False)
        port.write(b"\x03\r")
        banner = mcu.ANSI.sub(b"", port.read_until_quiet(2.0, quiet=0.4))
        # The probe prints its own line every time the port is opened
        banner = b"\n".join(line for line in banner.split(b"\n") if b"fixed baud rate" not in line).strip()
        any_output = any_output or bool(banner)
        if SHELL_PROMPT.search(banner):
            return True, True
        if not banner:
            break  # a silent console stays silent; do not spend more time on it
    return False, any_output


def host_exec(command, timeout=30.0, wait=0.0):
    """Run a shell command on the Linux host over its serial console. Returns (output, exit code)."""
    marker = "__F1_DONE__"
    done = re.compile(marker.encode() + rb"(\d+)\r?\n")
    with open_role("linux") as port:
        deadline = time.monotonic() + wait
        while True:
            ready, any_output = host_shell_ready(port)
            if ready:
                break
            if time.monotonic() >= deadline:
                state = "shows no shell prompt: the host is still booting or at a login prompt" if any_output else "is silent: the host is off or waiting in the boot menu"
                raise RuntimeError(f"Linux console {state}. Check 'f1.py linux state' and 'f1.py screenshot', or retry with --wait SECONDS")
            time.sleep(2)
        port.write(f"stty -echo cols 250; {command}; echo {marker}$?; stty echo\r".encode())
        try:
            reply = port.read_until(lambda d: done.search(d), timeout)
        except TimeoutError as error:
            raise TimeoutError(f"Command did not finish in {timeout:.0f}s. Output so far:\n{mcu.clean(error.data)}") from None
        port.read_until_quiet(0.5, quiet=0.2, wait_for_data=False)
    code = int(done.search(reply).group(1))
    body = mcu.clean(reply[: done.search(reply).start()])
    first, _, rest = body.partition("\n")
    if "stty -echo" in first:  # the echo of the command line itself
        body = rest
    return body.strip("\n"), code


def cmd_host(args):
    output, code = host_exec(args.command, args.timeout, args.wait)
    print(output)
    if code:
        print(f"[exit code {code}]")
    return code


def running_mcu_app(console):
    """App id running on the MCU desktop, or None."""
    reply = console.command("desktop get_run_app")
    return reply.split(":", 1)[1].strip() if "running app:" in reply else None


def mcu_app(args):
    with open_mcu_console(args) as port:
        console = mcu.McuConsole(port)
        console.sync()
        return running_mcu_app(console)


def mcu_cli(args, line):
    with open_mcu_console(args) as port:
        console = mcu.McuConsole(port)
        console.sync()
        return console.command(line)


def host_shell_state():
    """(shell answers, console shows any output)"""
    with open_role("linux") as port:
        return host_shell_ready(port)


def clean_poweroff(timeout):
    """Ask Linux to power off and wait for the kernel to say it did.

    Nothing is typed into the console while waiting: a Ctrl+C there would
    interrupt the very command that is shutting the system down.
    """
    with open_role("linux") as port:
        if not host_shell_ready(port)[0]:
            raise RuntimeError("Linux console shows no shell prompt, a clean shutdown is not possible; use 'f1.py linux stop --hard'")
        deadline = time.monotonic() + timeout
        under_way = False
        while True:
            if not under_way:
                port.write(b"sync; sudo -n poweroff\r")
            try:
                port.read_until(lambda d: b"Power down" in d, min(30.0, max(deadline - time.monotonic(), 1.0)))
                return
            except TimeoutError as error:
                if time.monotonic() >= deadline:
                    raise RuntimeError(f"Linux did not power down in {timeout:.0f}s; use 'f1.py linux stop --hard'. Last console output:\n{mcu.clean(error.data)[-300:]}") from None
                # systemd ignores a poweroff issued while the system is still
                # starting up, so it is sent again unless a shutdown has begun
                under_way = under_way or b"power off now" in error.data


def boot_entry_position(wanted, entries):
    """Menu position (from 1) for --entry: a number, or text found in exactly one entry name."""
    if wanted.isdigit():
        position = int(wanted)
        if position < 1 or (entries and position > len(entries)):
            raise RuntimeError(f"There is no boot entry {position}; the host has {len(entries)}")
        return position
    matching = [number for number, name in enumerate(entries, 1) if wanted.lower() in name.lower()]
    if len(matching) != 1:
        raise RuntimeError(f"--entry '{wanted}' matches {len(matching)} boot entries, it has to match one; the host is powered off again")
    return matching[0]


def host_uptime(output):
    """Seconds from the output of `cut -d' ' -f1 /proc/uptime`; kernel messages may be mixed into it."""
    numbers = re.findall(r"^\d+\.\d+$", output, re.M)
    if not numbers:
        raise RuntimeError(f"Could not read the host uptime from: {output[-200:]}")
    return float(numbers[-1])


def cmd_linux(args):
    # The host rails are held by an I/O expander that keeps its outputs across an
    # MCU soft reset. So the host can be up while the MCU's host app is not
    # running: the MCU was reset under a running Linux and shows its own desktop.
    if args.action == "state":
        app = mcu_app(args)
        ready, any_output = host_shell_state()
        if app == "cpu_app_maskrom":
            print("powered in maskrom mode: the host SoC waits for a loader over its own USB port and prints nothing. "
                  "'f1.py cli \"desktop stop_app\"' powers it off")
        elif app == CPU_APP:
            print("up: shell answers" if ready else "powered, no shell prompt: booting, waiting in the boot menu, or at a login prompt")
        elif ready:
            print("up: shell answers, but the MCU host app is not running (the MCU was reset while Linux ran). "
                  "The device screen and buttons belong to the MCU, not to Linux. 'f1.py linux start' attaches to the running Linux")
        elif any_output:
            print("powered, no shell prompt, and the MCU host app is not running (the MCU was reset while the host was booting, in its boot menu or at a login prompt)")
        else:
            print("off" + (f" (MCU is running app '{app}')" if app else ""))

    elif args.action == "start":
        app = mcu_app(args)
        if app == CPU_APP:
            print("host is already powered; see 'f1.py linux state'")
            return
        if app:
            raise RuntimeError(f"MCU is running app '{app}'; close it first (press back, or 'f1.py cli \"desktop stop_app\"')")
        if host_shell_state()[0]:
            # The MCU was reset while Linux ran. The host rails are held by the
            # I/O expander, and the host app leaves a host alone that the expander
            # shows as powered. Now and then the MCU fails to initialise the
            # expander when it boots, on the bus shared with the host; it then has
            # no control over host power until it is reset again.
            if "Main Expander" in mcu_cli(args, "dmesg"):
                raise RuntimeError(
                    "Linux is up, but this MCU boot failed to initialise the main expander, so the MCU cannot control host power. "
                    "Reset the MCU again ('f1.py swd reset') and retry"
                )
            before = host_uptime(host_exec("sync; cut -d' ' -f1 /proc/uptime", timeout=20)[0])
            reply = mcu_cli(args, f"desktop start_app {CPU_APP}")
            if "started" not in reply:
                raise RuntimeError(f"MCU refused to start the host app: {reply}")
            time.sleep(3.0)
            try:
                after = host_uptime(host_exec("cut -d' ' -f1 /proc/uptime", timeout=10)[0])
            except (RuntimeError, ValueError):
                after = 0.0
            if after <= before:
                raise RuntimeError("The MCU power-cycled the host instead of attaching to it (a sync was done first). The host is restarting; run 'f1.py linux stop --hard' and 'f1.py linux start'")
            print(f"attached to the running Linux, no restart (host uptime {after:.0f}s)")
            return
        with open_role("linux") as host:
            reply = mcu_cli(args, f"desktop start_app {CPU_APP}")
            if "started" not in reply:
                raise RuntimeError(f"MCU refused to start the host: {reply}")
            host.read_until_quiet(1.0, quiet=0.5, wait_for_data=False)  # the probe's own banner
            lost_control = "The MCU may have lost control of host power: that happens when it was reset while Linux ran. Reset the MCU with the host off ('f1.py swd reset', then close the self-check with 'f1.py press ok') and start again."
            if not host.read_until_quiet(8.0, quiet=0.2):
                # The host app only powers the host if it finds the rails off. If
                # Linux powered itself down while the app was not running, the
                # rails still read as on and nothing happens: cut and start again.
                mcu_cli(args, "desktop stop_app")
                time.sleep(1.5)
                reply = mcu_cli(args, f"desktop start_app {CPU_APP}")
                if "started" not in reply:
                    raise RuntimeError(f"MCU refused to start the host: {reply}")
            # U-Boot lists what it found, then draws a menu and waits for a key
            try:
                scan = mcu.clean(host.read_until(lambda d: b"bootflows," in d, 40.0))
            except TimeoutError as error:
                seen = mcu.clean(error.data).strip()
                if not seen:
                    mcu_cli(args, "desktop stop_app")
                    raise RuntimeError("The host shows no sign of life after power-on. " + lost_control) from None
                raise RuntimeError(f"No U-Boot boot list on the Linux console in 40s. Last output:\n{seen[-400:]}") from None
            # The menu shows the boot loader entries in the order U-Boot lists them
            entries = re.findall(r"/loader/entries/(?:\d+-)?(?:flipperos-)?(\S+)\.conf", scan)
            for number, name in enumerate(entries, 1):
                print(f"boot entry {number}: {name}")
            try:
                position = boot_entry_position(args.entry, entries)
            except RuntimeError:
                mcu_cli(args, "desktop stop_app")
                raise
            time.sleep(1.0)
            host.write(b"\x1b[B" * (position - 1) + b"\r")
            booting = mcu.clean(host.read_until_quiet(8.0, quiet=1.5))
        chosen = next((line.lstrip(":").strip() for line in booting.split("\n") if line.startswith(":")), None)
        if "Boot failed" in booting:
            raise RuntimeError(
                f"U-Boot could not boot entry {position}" + (f" ({chosen})" if chosen else "") + "; the host sits at the U-Boot prompt. "
                "Run 'f1.py linux stop --hard' and start another entry. U-Boot said:\n" + booting.strip()[-300:]
            )
        print(f"boot menu entry {position}" + (f" ({chosen})" if chosen else "") + " selected")
        if args.no_wait:
            return
        output, _ = host_exec("uname -r", wait=args.timeout)
        print(f"host is up, kernel {output}")

    elif args.action == "stop":
        app = mcu_app(args)
        ready, _ = host_shell_state()
        if app != CPU_APP and not ready:
            print("host is already off")
            return
        if args.hard:
            if ready:
                host_exec("sync", timeout=20)
            else:
                print("note: host shell did not answer, power is cut without a sync")
            if app != CPU_APP:
                # Power is cut by the host app on its way out, so it has to be running
                reply = mcu_cli(args, f"desktop start_app {CPU_APP}")
                if "started" not in reply:
                    raise RuntimeError(f"Cannot attach the MCU host app to stop the host: {reply}")
                time.sleep(1.0)
            mcu_cli(args, "desktop stop_app")
        else:
            clean_poweroff(args.timeout)
        deadline = time.monotonic() + 20.0
        while mcu_app(args) == CPU_APP:
            if time.monotonic() >= deadline:
                raise RuntimeError("Host power is down but the MCU host app is still running; check 'f1.py screenshot'")
            time.sleep(1.0)
        print("host is off (power cut by the MCU)" if args.hard else "host is off (clean shutdown)")


def cmd_swd(args):
    if args.action == "status":
        args.no_swd = False
        return cmd_status(args)
    if args.action == "reset":
        swd.reset()
        print("MCU reset (SYSRESETREQ)")
    elif args.action == "resume":
        swd.resume()
        print("cores resumed")
    elif args.action == "where":
        for core, (pc, lr) in sorted(swd.where().items()):
            (_, pc_name), (_, lr_name) = swd.symbolize([pc, lr])
            print(f"core{core}  pc {pc:#010x} {pc_name}")
            print(f"       lr {lr:#010x} {lr_name}")
    elif args.action == "symbol":
        for address, name in swd.symbolize([int(word, 16) for word in args.addresses]):
            print(f"{address:#010x} {name}")
    elif args.action == "read":
        words = swd.read_words([(int(args.address, 0), args.count)])[0]
        print(" ".join(f"{word:08x}" for word in words))
    elif args.action == "flash":
        if not args.yes:
            raise RuntimeError("Flashing overwrites the slot and halts the MCU while writing; pass --yes once the user agreed")
        swd.flash_slot(args.slot, args.file)
        print(f"wrote {args.file} to slot {args.slot.upper()}, verified, MCU reset")
    elif args.action == "erase-header":
        if not args.yes:
            raise RuntimeError("This makes the slot unbootable; pass --yes once the user agreed")
        swd.erase_slot_header(args.slot)
        print(f"erased the first sector of slot {args.slot.upper()}, MCU reset")


# --- argument parsing -------------------------------------------------------


def build_parser():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="cmd", required=True)

    sub.add_parser("ports", help="list serial ports by role").set_defaults(func=cmd_ports)

    def add_console_option(p):
        p.add_argument("--console", choices=["mcu", "mcu-usb"], default="mcu", help="MCU console to use (default: mcu, the UART one)")

    p = sub.add_parser("status", help="ports, whether Linux runs, MCU state and firmware slots")
    p.add_argument("--no-swd", action="store_true", help="skip the SWD part (the probe may be busy)")
    p.set_defaults(func=cmd_status)

    p = sub.add_parser("cli", help="run MCU console commands")
    p.add_argument("command", nargs="+", help="one CLI command per argument, e.g. 'uptime' 'power show'")
    p.add_argument("--timeout", type=float, default=5.0)
    add_console_option(p)
    p.set_defaults(func=cmd_cli)

    p = sub.add_parser("probe", help="run commands on the debug probe's own console")
    p.add_argument("command", nargs="+", help="one command per argument, e.g. 'device_info', 'power reboot'; or: flash FILE.uf2 (reflash the probe)")
    p.add_argument("--timeout", type=float, default=5.0)
    p.add_argument("--yes", action="store_true", help="confirm 'flash'")
    p.set_defaults(func=cmd_probe)

    def add_shot_options(p):
        p.add_argument("--scale", type=int, default=3, help="enlarge the image this many times (default 3)")
        add_console_option(p)

    p = sub.add_parser("screenshot", help="save the MCU screen as a PNG")
    p.add_argument("out", help="output .png path")
    add_shot_options(p)
    p.set_defaults(func=cmd_screenshot)

    def add_after_options(p):
        p.add_argument("--shot", metavar="PNG", help="take a screenshot afterwards, in the same session")
        p.add_argument("--settle", type=float, default=0.3, help="seconds to wait before that screenshot")
        add_shot_options(p)

    p = sub.add_parser("press", help="press buttons, one after another")
    p.add_argument("button", nargs="+", choices=sorted(mcu.BUTTONS))
    p.add_argument("--hold", type=float, default=0.0, help="hold for this many seconds: sends long instead of short")
    p.add_argument("--repeats", type=int, default=0, help="with --hold: number of repeat events to send")
    p.add_argument("--action", choices=sorted(mcu.ACTIONS), help="send only this raw event instead of a full click")
    p.add_argument("--delay", type=float, default=0.15, help="seconds between buttons")
    add_after_options(p)
    p.set_defaults(func=cmd_press)

    p = sub.add_parser("touch", help="tap or swipe on the touchpad (x 0..1023, y 0..767, centre 512 384)")
    p.add_argument("x", type=int, help="0..1023, left to right")
    p.add_argument("y", type=int, help="0..767, top to bottom")
    p.add_argument("--to", type=int, nargs=2, metavar=("X", "Y"), help="swipe to this point")
    p.add_argument("--steps", type=int, default=10, help="move events along a swipe")
    p.add_argument("--pressure", type=int, default=8000, help="0..16383 (default 8000)")
    add_after_options(p)
    p.set_defaults(func=cmd_touch)

    p = sub.add_parser("log", help="print the MCU log with host timestamps")
    p.add_argument("--seconds", type=float, help="stop after this long (default: run until killed)")
    p.add_argument("--out", help="append to this file instead of printing")
    p.set_defaults(func=cmd_log)

    p = sub.add_parser("host", help="run a shell command on the Linux host over its serial console")
    p.add_argument("command")
    p.add_argument("--timeout", type=float, default=30.0, help="seconds the command may run")
    p.add_argument("--wait", type=float, default=0.0, help="keep trying this long for a shell prompt, e.g. while the host boots")
    p.set_defaults(func=cmd_host)

    p = sub.add_parser("linux", help="start, stop or query the Linux host")
    linux_sub = p.add_subparsers(dest="action", required=True)
    q = linux_sub.add_parser("state", help="off, powered without a shell, or up")
    q = linux_sub.add_parser("start", help="power the host, pick a boot menu entry, wait for the shell")
    q.add_argument("--entry", default="1", help="boot menu entry: its number from 1, or text that one entry name contains, e.g. Minimal-7 (default 1)")
    q.add_argument("--no-wait", action="store_true", help="return once the entry is selected")
    q.add_argument("--timeout", type=float, default=120.0, help="seconds to wait for the shell")
    q = linux_sub.add_parser("stop", help="clean shutdown; --hard cuts power from the MCU")
    q.add_argument("--hard", action="store_true", help="cut power without asking Linux (a sync is attempted first)")
    q.add_argument("--timeout", type=float, default=90.0, help="seconds to wait for the host to go off")
    p.set_defaults(func=cmd_linux)

    p = sub.add_parser("swd", help="SWD through OpenOCD")
    swd_sub = p.add_subparsers(dest="action", required=True)
    swd_sub.add_parser("status", help="same as 'status'")
    swd_sub.add_parser("reset", help="plain MCU reset; the host keeps running")
    swd_sub.add_parser("resume", help="resume cores left halted by a debugger")
    swd_sub.add_parser("where", help="halt for a moment, print pc and lr of both cores with function names, run on")
    q = swd_sub.add_parser("symbol", help="function and source line for addresses from a crash log (uses build/*.elf, no probe needed)")
    q.add_argument("addresses", nargs="+", help="hex addresses, e.g. 1000a133")
    q = swd_sub.add_parser("read", help="read 32-bit words without halting")
    q.add_argument("address")
    q.add_argument("count", type=int, nargs="?", default=1)
    q = swd_sub.add_parser("flash", help="write a .bin to a firmware slot, verify, reset")
    q.add_argument("slot", choices=["a", "b", "A", "B"])
    q.add_argument("file")
    q.add_argument("--yes", action="store_true")
    q = swd_sub.add_parser("erase-header", help="invalidate a slot by erasing its first sector")
    q.add_argument("slot", choices=["a", "b", "A", "B"])
    q.add_argument("--yes", action="store_true")
    p.set_defaults(func=cmd_swd)

    return parser


def main():
    args = build_parser().parse_args()
    try:
        return args.func(args) or 0
    except (RuntimeError, TimeoutError, ValueError, OSError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
