#!/usr/bin/env python3
"""Tools for an LLM agent to see and drive a Flipper One on the bench.

Every command is non-interactive, has a timeout, prints plain text and exits
non-zero with a one-line reason on failure. See AGENT_TOOLS.md next to this file.
"""

import argparse
import codecs
import contextlib
import json
import os
import re
import subprocess
import sys
import time
import warnings

from .. import constants
from ..logic import flash, swd
from ..logic.device import DeviceSession
from ..logic.linux import LinuxHost
from ..logic.rpc import write_png
from ..packets import host_shell, mcu_console, rpc, uboot
from ..transport import ports
from ..transport.serialport import SerialPort


def open_role(role):
    return SerialPort(ports.require(role), ports.baud(role))


# --- JSON output ------------------------------------------------------------


def command_name(args):
    action = getattr(args, "action", None)
    return f"{args.cmd} {action}" if action else args.cmd


def envelope(args, data=None, error=None):
    body = {"tool": "f1.py", "command": command_name(args), "ok": error is None}
    if error is None:
        body["data"] = data
    else:
        body["error"] = error
    return body


def emit(args, data, render):
    if getattr(args, "json", False):
        print(json.dumps(envelope(args, data=data)))
    else:
        render(data)


# --- commands ---------------------------------------------------------------


def probe_info():
    """Structured one-liner about the selected probe's firmware, from its own console."""
    probe = ports.selected_probe()
    if not probe:
        return {"connected": False, "text": "not connected"}
    try:
        with DeviceSession("probe") as session:
            info = session.command("device_info")
    except (OSError, TimeoutError, RuntimeError) as error:
        return {"connected": True, "id": probe["id"], "text": f"{probe['id']}, console did not answer ({error})"}
    fields = dict(re.findall(r"firmware_(version|commit|commit_dirty)\s*:\s*(\S+)", info))
    firmware = {"version": fields.get("version", "?"), "commit": fields.get("commit", "?"), "dirty": fields.get("commit_dirty") == "true"}
    text = f"{probe['id']}, firmware {firmware['version']} commit {firmware['commit']}" + (" dirty" if firmware["dirty"] else "")
    return {"connected": True, "id": probe["id"], "firmware": firmware, "text": text}


def ports_data():
    attached = ports.probes()
    several = len(attached) > 1 and not os.environ.get("F1_PROBE")
    data = {"probes": attached, "several": several, "roles": {}, "mcu_usb_present": bool(ports.find("mcu-usb"))}
    if not several:
        for role in ports.ROLES:
            data["roles"][role] = {"path": ports.find(role), "baud": ports.baud(role), "description": ports.ROLES[role][3]}
    return data


def render_ports(data):
    for probe in data["probes"]:
        print(f"probe {probe['id']}  serial {probe['serial']}")
    if data["several"]:
        print("several probes attached: set F1_PROBE=<ID or its beginning> to choose one")
        return
    for role, info in data["roles"].items():
        print(f"{role:8} {info['path'] or '-':44} {info['baud']:>8}  {info['description']}")
    if data["mcu_usb_present"]:
        print("mcu-usb is present: Linux is not running")


def cmd_ports(args):
    emit(args, ports_data(), render_ports)


def serial_data():
    return {
        "probe": probe_info(),
        "ports": {role: ports.find(role) for role in ports.ROLES},
        "linux": "not_running" if ports.find("mcu-usb") else "may_be_running",
    }


def status_data(args):
    data = serial_data()
    if not args.no_swd:
        data["swd"] = swd.status()
    return data


def render_serial(data):
    print("probe    " + data["probe"]["text"])
    for role, path in data["ports"].items():
        print(f"port {role:8} {path or 'not connected'}")
    if data["linux"] == "not_running":
        print("linux    not running (mcu-usb port is present)")
    else:
        print("linux    may be running (no mcu-usb port)")


def render_swd(state):
    print("mcu      " + ("HALTED: by a debugger, or by the firmware after a crash (see the log)" if state["halted"] else "running") + f", uptime {state['uptime_s']:.1f}s")
    print("active   " + (f"slot {state['active_slot']}" if state["active_slot"] else "none: nothing booted, core is in the bootrom"))
    for name, slot in state["slots"].items():
        line = f"slot {name}   {slot['offset']:#08x} {slot['state']}"
        if slot["state"] == "valid":
            line += f", version words {slot.get('version_major_word', 0):#06x}/{slot.get('version_minor_word', 0)}"
            if slot.get("tbyb_pending"):
                line += ", try-before-you-buy not bought (ignored on a normal boot)"
        print(line)


def render_status(data):
    render_serial(data)
    if data.get("swd") is not None:
        render_swd(data["swd"])


def cmd_status(args):
    if getattr(args, "json", False):
        emit(args, status_data(args), render_status)
        return
    # Text mode prints the serial part first, so a busy probe still shows the ports.
    render_serial(serial_data())
    if not args.no_swd:
        render_swd(swd.status())


def decode_escapes(text):
    """Bytes of `text` with its backslash escapes; an unknown escape is an error, not a warning."""
    with warnings.catch_warnings():
        warnings.simplefilter("error", DeprecationWarning)
        try:
            return codecs.escape_decode(text.encode())[0]
        except (DeprecationWarning, ValueError) as error:
            raise ValueError(f"bad escape in {text!r}: {error}") from None


def cmd_cli(args):
    # Decode every argument first, so that a bad escape fails before anything is sent
    raw = [decode_escapes(line) for line in args.command] if args.raw else None
    with DeviceSession(args.console) as session:
        if raw:
            for data in raw:
                output = session.send_raw(data, args.timeout)
                if output:
                    print(output)
            return
        for line in args.command:
            if len(args.command) > 1:
                print(f"> {line}")
            print(session.command(line, args.timeout))


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
    with DeviceSession("probe") as session:
        try:
            session.command("power boot", 3.0)
        except (OSError, TimeoutError):
            pass
    drive = wait_for(bootloader_drive, 15.0, "the probe's bootloader drive did not appear")
    with open(path, "rb") as source:
        image = source.read()
    try:
        with open(os.path.join(drive, os.path.basename(path)), "wb") as target:
            target.write(image)
    except OSError:
        if bootloader_drive():  # still mounted: the copy failed before the drive went away
            raise
    wait_for(lambda: any(p["id"] == probe["id"] and p["ports"] for p in ports.probes()), 20.0, f"probe {probe['id']} did not come back")
    time.sleep(1.0)
    with DeviceSession("probe") as session:
        info = session.command("device_info")
    version = re.search(r"firmware_version\s*:\s*(\S+)", info)
    commit = re.search(r"firmware_commit\s*:\s*(\S+)", info)
    print(f"probe {probe['id']} flashed and back" + (f": version {version.group(1)}, commit {commit.group(1) if commit else '?'}" if version else ""))


def cmd_probe(args):
    """Commands on the console of the debug probe itself."""
    if args.command[0] == "flash":
        if len(args.command) != 2:
            raise RuntimeError("usage: f1.py probe flash FILE.uf2 --yes")
        return probe_flash(args, args.command[1])
    with DeviceSession("probe") as session:
        for line in args.command:
            if len(args.command) > 1:
                print(f"> {line}")
            leaving = line.split() in (["power", "boot"], ["power", "reboot"])
            try:
                reply = session.command(line, args.timeout)
            except (OSError, TimeoutError):
                if not leaving:
                    raise
                reply = ""
            if leaving:
                # the probe drops off USB before or after it replies
                print("the probe is restarting" if "reboot" in line else "the probe is in its USB bootloader: copy a .uf2 onto the RP2350 drive")
                return
            print(reply)


def cmd_screenshot(args):
    with DeviceSession(args.console) as session, session.rpc() as rpc_session:
        width, height, pixels = rpc_session.screenshot()
    write_png(args.out, width, height, pixels, args.scale)
    print(f"{args.out}: {width}x{height} screen, saved at {args.scale}x")


def report_port_lost(session):
    if session.port_lost:
        print("note: the MCU USB console disappeared during the command (expected when Linux powers up); use the default UART console")


def cmd_press(args):
    with DeviceSession(args.console) as session, session.rpc() as rpc_session:
        for button in args.button:
            if args.action:
                rpc_session.button(button, args.action)
            else:
                rpc_session.click(button, hold=args.hold, repeats=args.repeats)
            time.sleep(args.delay)
        if args.shot:
            time.sleep(args.settle)
            width, height, pixels = rpc_session.screenshot()
    print("sent: " + " ".join(args.button))
    report_port_lost(session)
    if args.shot:
        write_png(args.shot, width, height, pixels, args.scale)
        print(f"{args.shot}: {width}x{height} screen, saved at {args.scale}x")


def cmd_touch(args):
    x0, y0 = args.x, args.y
    x1, y1 = args.to if args.to else (x0, y0)
    for x, y in ((x0, y0), (x1, y1)):
        if not (0 <= x <= rpc.TOUCH_MAX_X and 0 <= y <= rpc.TOUCH_MAX_Y):
            raise ValueError(f"Touch point {x},{y} is outside 0..{rpc.TOUCH_MAX_X} x 0..{rpc.TOUCH_MAX_Y}")
    if not (0 <= args.pressure <= rpc.TOUCH_MAX_PRESSURE):
        raise ValueError(f"Pressure {args.pressure} is outside 0..{rpc.TOUCH_MAX_PRESSURE}")
    with DeviceSession(args.console) as session, session.rpc() as rpc_session:
        rpc_session.touch("start", x0, y0, args.pressure)
        steps = args.steps if args.to else 0
        for step in range(1, steps + 1):
            time.sleep(0.02)
            rpc_session.touch("move", x0 + (x1 - x0) * step // steps, y0 + (y1 - y0) * step // steps, args.pressure)
        time.sleep(0.05)
        rpc_session.touch("end", x1, y1, 0)
        if args.shot:
            time.sleep(args.settle)
            width, height, pixels = rpc_session.screenshot()
    print(f"touch {x0},{y0}" + (f" -> {x1},{y1}" if args.to else ""))
    if args.shot:
        write_png(args.shot, width, height, pixels, args.scale)
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
                    line = f"{time.time():.2f} {mcu_console.clean(raw)}"
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


def cmd_host(args):
    with LinuxHost() as host:
        output, code = host.exec(args.command, args.timeout, args.wait)
    print(output)
    if code:
        print(f"[exit code {code}]")
    return code


def cmd_linux(args):
    # The host rails survive an MCU reset (I/O expander), so a host can run with no
    # MCU host app: the MCU was reset under a running Linux.
    with DeviceSession(getattr(args, "console", None)) as mcu, LinuxHost() as host:
        if args.action == "state":
            app = mcu.app()
            ready, any_output = host.state()
            if app == "cpu_app_maskrom":
                key = "maskrom"
                text = ("powered in maskrom mode: the host SoC waits for a loader over its own USB port and prints nothing. "
                        "'f1.py cli \"desktop stop_app\"' powers it off")
            elif app == constants.CPU_APP:
                key = "up" if ready else "powered_no_shell"
                text = "up: shell answers" if ready else "powered, no shell prompt: booting, waiting in the boot menu, or at a login prompt"
            elif ready:
                key = "up_no_host_app"
                text = ("up: shell answers, but the MCU host app is not running (the MCU was reset while Linux ran). "
                        "The device screen and buttons belong to the MCU, not to Linux. 'f1.py linux start' attaches to the running Linux")
            elif any_output:
                key = "powered_no_shell_no_host_app"
                text = "powered, no shell prompt, and the MCU host app is not running (the MCU was reset while the host was booting, in its boot menu or at a login prompt)"
            else:
                key = "off"
                text = "off" + (f" (MCU is running app '{app}')" if app else "")
            emit(args, {"state": key, "app": app}, lambda data: print(text))

        elif args.action == "start":
            app = mcu.app()
            if app == constants.CPU_APP:
                print("host is already powered; see 'f1.py linux state'")
                return
            if app:
                raise RuntimeError(f"MCU is running app '{app}'; close it first (press back, or 'f1.py cli \"desktop stop_app\"')")
            if host.state()[0]:
                # The MCU was reset while Linux ran. The host app normally attaches to
                # the running host; rarely the MCU failed to initialise the expander on
                # boot and then cannot control host power until the next reset.
                if "Main Expander" in mcu.command("dmesg"):
                    raise RuntimeError(
                        "Linux is up, but this MCU boot failed to initialise the main expander, so the MCU cannot control host power. "
                        "Reset the MCU again ('f1.py swd reset') and retry"
                    )
                before = host_shell.parse_uptime(host.exec("sync; cut -d' ' -f1 /proc/uptime", timeout=20)[0])
                reply = mcu.command(f"desktop start_app {constants.CPU_APP}")
                if "started" not in reply:
                    raise RuntimeError(f"MCU refused to start the host app: {reply}")
                time.sleep(3.0)
                try:
                    after = host_shell.parse_uptime(host.exec("cut -d' ' -f1 /proc/uptime", timeout=10)[0])
                except (RuntimeError, ValueError):
                    after = 0.0
                if after <= before:
                    raise RuntimeError("The MCU power-cycled the host instead of attaching to it (a sync was done first). The host is restarting; run 'f1.py linux stop --hard' and 'f1.py linux start'")
                print(f"attached to the running Linux, no restart (host uptime {after:.0f}s)")
                return
            port = host.port()
            reply = mcu.command(f"desktop start_app {constants.CPU_APP}")
            if "started" not in reply:
                raise RuntimeError(f"MCU refused to start the host: {reply}")
            port.read_until_quiet(1.0, quiet=0.5, wait_for_data=False)  # the probe's own banner
            lost_control = "The MCU may have lost control of host power: that happens when it was reset while Linux ran. Reset the MCU with the host off ('f1.py swd reset', then close the self-check with 'f1.py press ok') and start again."
            if not port.read_until_quiet(8.0, quiet=0.2):
                # The host app only powers a host whose rails read off; if Linux
                # powered itself down meanwhile, cut and start again.
                mcu.command("desktop stop_app")
                time.sleep(1.5)
                reply = mcu.command(f"desktop start_app {constants.CPU_APP}")
                if "started" not in reply:
                    raise RuntimeError(f"MCU refused to start the host: {reply}")
            # U-Boot scans, prints its list, then waits in a menu for a key
            try:
                scan = mcu_console.clean(port.read_until(lambda d: b"bootflows," in d, 40.0))
            except TimeoutError as error:
                seen = mcu_console.clean(error.data).strip()
                if not seen:
                    mcu.command("desktop stop_app")
                    raise RuntimeError("The host shows no sign of life after power-on. " + lost_control) from None
                raise RuntimeError(f"No U-Boot boot list on the Linux console in 40s. Last output:\n{seen[-400:]}") from None
            # Entry numbers follow the order U-Boot listed them in
            entries = uboot.parse_boot_entries(scan)
            for number, name in enumerate(entries, 1):
                print(f"boot entry {number}: {name}")
            try:
                position = uboot.entry_position(args.entry, entries)
            except RuntimeError:
                mcu.command("desktop stop_app")
                raise
            time.sleep(1.0)
            port.write(b"\x1b[B" * (position - 1) + b"\r")
            booting = mcu_console.clean(port.read_until_quiet(8.0, quiet=1.5))
            chosen = next((line.lstrip(":").strip() for line in booting.split("\n") if line.startswith(":")), None)
            if "Boot failed" in booting:
                raise RuntimeError(
                    f"U-Boot could not boot entry {position}" + (f" ({chosen})" if chosen else "") + "; the host sits at the U-Boot prompt. "
                    "Run 'f1.py linux stop --hard' and start another entry. U-Boot said:\n" + booting.strip()[-300:]
                )
            print(f"boot menu entry {position}" + (f" ({chosen})" if chosen else "") + " selected")
            if args.no_wait:
                return
            output, _ = host.exec("uname -r", wait=args.timeout)
            print(f"host is up, kernel {output}")

        elif args.action == "stop":
            app = mcu.app()
            ready, _ = host.state()
            if app != constants.CPU_APP and not ready:
                print("host is already off")
                return
            if args.hard:
                if ready:
                    host.exec("sync", timeout=20)
                else:
                    print("note: host shell did not answer, power is cut without a sync")
                if app != constants.CPU_APP:
                    # Power is cut by the host app on its way out, so it has to be running
                    reply = mcu.command(f"desktop start_app {constants.CPU_APP}")
                    if "started" not in reply:
                        raise RuntimeError(f"Cannot attach the MCU host app to stop the host: {reply}")
                    time.sleep(1.0)
                mcu.command("desktop stop_app")
            else:
                host.poweroff(args.timeout)
            deadline = time.monotonic() + 20.0
            while mcu.app() == constants.CPU_APP:
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
        data = {"address": int(args.address, 0), "count": args.count, "words": [f"{word:08x}" for word in words]}
        emit(args, data, lambda d: print(" ".join(d["words"])))
    elif args.action == "flash":
        if not args.yes:
            raise RuntimeError("Flashing overwrites the slot and halts the MCU while writing; pass --yes once the user agreed")
        # Before freeze_host(): a rejected image must not reset a running host
        plan = flash.preflight(args.slot, args.file)
        with contextlib.ExitStack() as stack:
            if args.quiesce:
                stack.enter_context(flash.freeze_host())
            plan = flash.write(plan, activate=args.activate)
        print(f"sha256 {plan['sha256']}")
        if plan["image"]["state"] != "valid":
            print("note: no valid picobin image header in the image; the bootrom will not boot it")
        elif plan["tbyb"] and plan["staged"]:
            print("try-before-you-buy image staged and bought: the MCU runs the new slot now")
        elif plan["tbyb"]:
            print("note: try-before-you-buy image, it will not boot on a normal reset; re-run with --activate")
        print(f"wrote {args.file} to slot {plan['slot']}, verified, MCU reset")
    elif args.action == "erase-header":
        if not args.yes:
            raise RuntimeError("This makes the slot unbootable; pass --yes once the user agreed")
        swd.erase_slot_header(args.slot)
        print(f"erased the first sector of slot {args.slot.upper()}, MCU reset")


# --- argument parsing -------------------------------------------------------


def build_parser():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="cmd", required=True)

    p = sub.add_parser("ports", help="list serial ports by role")
    p.add_argument("--json", action="store_true", help="print a JSON envelope instead of text")
    p.set_defaults(func=cmd_ports)

    def add_console_option(p):
        p.add_argument("--console", choices=["mcu", "mcu-usb"], default="mcu", help="MCU console to use (default: mcu, the UART one)")

    p = sub.add_parser("status", help="ports, whether Linux runs, MCU state and firmware slots")
    p.add_argument("--no-swd", action="store_true", help="skip the SWD part (the probe may be busy)")
    p.add_argument("--json", action="store_true", help="print a JSON envelope instead of text")
    p.set_defaults(func=cmd_status)

    p = sub.add_parser("cli", help="run MCU console commands")
    p.add_argument("command", nargs="+", help="one CLI command per argument, e.g. 'uptime' 'power show'")
    p.add_argument("--timeout", type=float, default=5.0)
    p.add_argument(
        "--raw",
        action="store_true",
        help="send each argument as bytes, with escapes such as \\r and \\x03, without Enter, after the usual sync; no prompt is awaited afterwards",
    )
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
    p.add_argument("button", nargs="+", choices=sorted(rpc.BUTTONS))
    p.add_argument("--hold", type=float, default=0.0, help="hold for this many seconds: sends long instead of short")
    p.add_argument("--repeats", type=int, default=0, help="with --hold: number of repeat events to send")
    p.add_argument("--action", choices=sorted(rpc.ACTIONS), help="send only this raw event instead of a full click")
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
    q.add_argument("--json", action="store_true", help="print a JSON envelope instead of text")
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
    q = swd_sub.add_parser("status", help="same as 'status'")
    q.add_argument("--json", action="store_true", help="print a JSON envelope instead of text")
    swd_sub.add_parser("reset", help="plain MCU reset; the host keeps running")
    swd_sub.add_parser("resume", help="resume cores left halted by a debugger")
    swd_sub.add_parser("where", help="halt for a moment, print pc and lr of both cores with function names, run on")
    q = swd_sub.add_parser("symbol", help="function and source line for addresses from a crash log (uses the newest ELF under build/, no probe needed)")
    q.add_argument("addresses", nargs="+", help="hex addresses, e.g. 1000a133")
    q = swd_sub.add_parser("read", help="read 32-bit words without halting")
    q.add_argument("address")
    q.add_argument("count", type=int, nargs="?", default=1)
    q.add_argument("--json", action="store_true", help="print a JSON envelope instead of text")
    q = swd_sub.add_parser("flash", help="write a .bin or slot-relocated .uf2 to a firmware slot, verify, reset")
    q.add_argument("slot", choices=["a", "b", "A", "B"])
    q.add_argument("file")
    q.add_argument("--yes", action="store_true")
    q.add_argument("--activate", action="store_true", help="try-before-you-buy image: boot it once so the bootrom stages and commits it")
    q.add_argument("--quiesce", action="store_true", help="hold the host CPU in reset through the probe while writing")
    q = swd_sub.add_parser("erase-header", help="invalidate a slot by erasing its first sector")
    q.add_argument("slot", choices=["a", "b", "A", "B"])
    q.add_argument("--yes", action="store_true")
    p.set_defaults(func=cmd_swd)

    return parser


def main():
    args = build_parser().parse_args()
    try:
        return args.func(args) or 0
    except (RuntimeError, TimeoutError, ValueError, OSError, subprocess.TimeoutExpired) as error:
        if getattr(args, "json", False):
            print(json.dumps(envelope(args, error=str(error))), file=sys.stderr)
        else:
            print(f"error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
