"""OpenOCD text output: mark extraction and failure classification."""

MARK = "F1>"


def extract_marks(output):
    """The lines printed with `echo "F1> ..."` in the OpenOCD script."""
    return [line[len(MARK) :].strip() for line in output.splitlines() if line.startswith(MARK)]


def error_lines(output):
    """Unique error lines, in the order they appear."""
    return list(dict.fromkeys(line for line in output.splitlines() if line.startswith("Error")))


def failure_hint(output):
    """A diagnosis suffix for a failed run, or an empty string."""
    if "unable to find a matching CMSIS-DAP device" in output or "no device found" in output.lower():
        return " The probe is not connected."
    if "CMD_INFO failed" in output or "LIBUSB_ERROR_ACCESS" in output or "could not claim" in output.lower() or "busy" in output.lower():
        return " The probe is probably held by another debugger."
    return ""
