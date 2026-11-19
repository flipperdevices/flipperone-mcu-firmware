"""Find the Flipper One serial ports by role, on any OS.

    python3 scripts/get_port.py            # list all roles
    python3 scripts/get_port.py mcu        # print one device path

Roles: linux, mcu, mcu-log, probe, mcu-usb. Ports are matched by USB serial and
CDC interface, since names differ per OS; override with F1_PORT_<ROLE>, and pick
one of several probes with F1_PROBE. The probe's UART bridges run at a fixed
1500000 baud.
"""

import glob
import os
import re
import sys

from .. import constants
from . import process

PROBE_VID, PROBE_PID = constants.PROBE_VID, constants.PROBE_PID
PROBE_SERIAL = constants.PROBE_SERIAL
PROBE_SERIAL_PATTERN = constants.PROBE_SERIAL_PATTERN
OLD_PROBE = constants.OLD_PROBE
MCU_VID, MCU_PID, MCU_SERIAL = constants.MCU_VID, constants.MCU_PID, constants.MCU_SERIAL
ROLES = constants.ROLES


def baud(role):
    return ROLES[role][2]


def _find_windows(serial, control_itf):
    try:
        from serial.tools import list_ports  # type: ignore
    except ImportError as exc:
        raise RuntimeError("Port lookup on Windows needs pyserial: pip install pyserial") from exc
    wanted = (PROBE_VID, PROBE_PID) if serial == PROBE_SERIAL else (MCU_VID, MCU_PID)
    for port in list_ports.comports():
        if (port.vid, port.pid) != wanted:
            continue
        if f"MI_{control_itf:02d}" in (port.hwid or "").upper() or (port.location or "").endswith(f".{control_itf}"):
            return port.device
    return None


# --- probes -----------------------------------------------------------------
# Serial "f1_debug_d950_": four hex digits folded from the chip ID (the tail of
# the USB product string); probe firmware 0.7.0+. F1_PROBE selects one of several.


def _ioreg(*args):
    return process.run(["ioreg", *args, "-l", "-w0"]).stdout


def _macos_usb_devices(vid, pid):
    """[(product string, locationID, serial number)] for attached devices with this VID:PID."""
    devices, current = [], {}
    for line in _ioreg("-p", "IOUSB").splitlines() + ["+-o end@0 "]:
        if re.search(r"\+-o .+@[0-9a-f]+ ", line):
            if current.get("idVendor") == str(vid) and current.get("idProduct") == str(pid):
                devices.append((current.get("kUSBProductString", "").strip('"'), int(current.get("locationID", "0")), current.get("kUSBSerialNumberString", "").strip('"')))
            current = {}
        match = re.search(r'"(idVendor|idProduct|kUSBProductString|kUSBSerialNumberString|locationID)" = (.+)', line)
        if match:
            current[match.group(1)] = match.group(2).strip()
    return devices


def _macos_ports(location):
    """{control interface number: tty path} for the CDC ports of the device at this locationID."""
    result, interface, at_location = {}, None, False
    for line in _ioreg("-r", "-c", "IOUSBHostInterface").splitlines():
        if "<class IOUSBHostInterface" in line:
            interface, at_location = None, False
        match = re.search(r'"(bInterfaceNumber|locationID|IOCalloutDevice)" = (.+)', line)
        if not match:
            continue
        key, value = match.group(1), match.group(2).strip().strip('"')
        if key == "bInterfaceNumber":
            interface = int(value)
        elif key == "locationID":
            at_location = int(value) == location
        elif at_location and interface is not None:
            result[interface - 1] = value  # the tty hangs off the data interface
    return result


def _linux_probes():
    """{(chip id, serial number): {control interface: tty path}} from /dev/serial/by-id."""
    found = {}
    for path in glob.glob("/dev/serial/by-id/*Debug_Probe_*-if*"):
        match = re.search(r"Debug_Probe_([0-9A-Fa-f]+)_(" + PROBE_SERIAL_PATTERN + r")-if(\d+)", path)
        if match:
            found.setdefault((match.group(1).upper(), match.group(2)), {})[int(match.group(3))] = path
    return found


def probes():
    """Attached probes: [{'id': chip id, 'serial': USB serial number, 'ports': {control interface: path}}]."""
    if sys.platform == "darwin":
        return [
            {"id": product.split()[-1].upper(), "serial": serial, "ports": _macos_ports(location)}
            for product, location, serial in _macos_usb_devices(PROBE_VID, PROBE_PID)
        ]
    if os.name == "nt":
        return []  # not implemented: use F1_PORT_<ROLE>
    return [
        {"id": chip_id, "serial": serial, "ports": tty}
        for (chip_id, serial), tty in sorted(_linux_probes().items())
    ]


def selected_probe():
    """The probe to use, or None if none is attached. Raises if the choice is ambiguous."""
    attached = probes()
    wanted = os.environ.get("F1_PROBE", "").upper()
    described = ", ".join(f"{probe['id']} ({probe['serial']})" for probe in attached) or "none"
    if wanted:
        matching = [probe for probe in attached if probe["id"].startswith(wanted) or probe["serial"] == f"f1_debug_{wanted.lower()}_"]
        if len(matching) != 1:
            raise RuntimeError(f"F1_PROBE={wanted} matches {len(matching)} probes; attached: {described}")
        chosen = matching[0]
    elif len(attached) > 1:
        raise RuntimeError(f"Several debug probes are attached: {described}. Pick one with F1_PROBE=<the four digits of its serial, or its ID or the beginning of it>")
    else:
        chosen = attached[0] if attached else None
    if chosen and not re.fullmatch(PROBE_SERIAL_PATTERN, chosen["serial"]):
        raise RuntimeError(f"Probe {chosen['id']} (serial {chosen['serial']}): {OLD_PROBE}")
    return chosen


def _find_mcu_usb():
    if sys.platform == "darwin":
        devices = _macos_usb_devices(MCU_VID, MCU_PID)
        if len(devices) != 1:
            return None  # with several boards there is no telling which MCU belongs to the probe
        return _macos_ports(devices[0][1]).get(0)
    matches = glob.glob(f"/dev/serial/by-id/*{MCU_SERIAL}-if00*")
    return matches[0] if len(matches) == 1 else None


def find(role):
    """Return the device path for a role, or None if it is not connected."""
    override = os.environ.get("F1_PORT_" + role.upper().replace("-", "_"))
    if override:
        return override
    serial, control_itf, _, _ = ROLES[role]
    if os.name == "nt":
        return _find_windows(serial, control_itf)
    if serial == MCU_SERIAL:
        return _find_mcu_usb()
    probe = selected_probe()
    return probe["ports"].get(control_itf) if probe else None


def require(role):
    path = find(role)
    if not path:
        raise RuntimeError(f"Port '{role}' ({ROLES[role][3]}) not found. Run 'scripts/get_port.py' to see what is connected.")
    return path


def mcu_console_role():
    """The UART console is always there. The MCU's own USB console exists only
    while Linux is off and vanishes mid-command when Linux powers up."""
    return "mcu"


def main(argv):
    if len(argv) > 1 and argv[1] in ("-h", "--help"):
        print(__doc__.strip())
        return 0
    try:
        if len(argv) > 1:
            if argv[1] not in ROLES:
                print(f"error: unknown role '{argv[1]}'; roles: {', '.join(ROLES)}", file=sys.stderr)
                return 2
            print(require(argv[1]))
            return 0
        for probe in probes():
            print(f"probe    {probe['id']}  serial {probe['serial']}")
        for role, (_, _, rate, description) in ROLES.items():
            print(f"{role:<8} {find(role) or '-':<36} {rate:>7}  {description}")
    except RuntimeError as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    return 0
