"""MCU update-over-I2C stress tests (bus and framing robustness).

Runs on the Flipper One host, MCU on /dev/i2c-0. Never resets the host or the
MCU: every case leaves the updater IDLE or recovers through ABORT.

Usage: sudo python3 updater_stress.py <firmware.bin> [--bus /dev/i2c-0]
"""

import os
import struct
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from updater import (
    McuI2C,
    Reg,
    ResetMode,
    UpdaterCommand,
    UpdaterError,
    UpdaterState,
    ensure_idle,
    get_boot_status,
    get_state_valid,
    iter_blocks,
    load_firmware,
    run_update,
    send_command,
    wait_for_state,
    UpdateError,
)

CHILD = r"""
import os, sys
sys.path.insert(0, "/tmp")
from updater import McuI2C, Reg, UpdaterCommand, UpdaterState, load_firmware, send_command, wait_for_state, iter_blocks

fw, blocks, fw_crc = load_firmware(sys.argv[1])
k = int(sys.argv[2]) if len(sys.argv) > 2 else 5
mcu = McuI2C()
mcu.write_register(Reg.FW_CRC, fw_crc)
mcu.write_register(Reg.FW_BLOCKS, blocks)
send_command(mcu, UpdaterCommand.START)
wait_for_state(mcu, UpdaterState.RUNNING, 10)
for i, frame in enumerate(iter_blocks(fw)):
    mcu.write_block(Reg.FW_DATA, frame)
    if i + 1 >= k:
        break
os._exit(1)
"""

stats = {"pass": 0, "fail": 0}


def report(name, ok, note=""):
    stats["pass" if ok else "fail"] += 1
    print(f"[{'PASS' if ok else 'FAIL'}] {name}" + (f": {note}" if note else ""))


def expect_state(mcu, state, timeout=10):
    actual, error = wait_for_state(mcu, state, timeout)
    return actual == state, f"state={actual.name} error={error.name}"


def start_update(mcu, firmware, blocks):
    mcu.write_register(Reg.FW_CRC, crc16(firmware))
    mcu.write_register(Reg.FW_BLOCKS, blocks)
    send_command(mcu, UpdaterCommand.START)


def crc16(data):
    from updater import crc16_xmodem

    return crc16_xmodem(data)


def reset_mcu(mcu):
    """Immediate MCU reset: the mode write is acked, then the MCU resets."""
    mcu.write_register(Reg.SYSTEM_RESET_MODE, ResetMode.RESET_MCU_IMMEDIATELY)
    # Let the MCU finish booting: bus traffic during boot wedged the host controller before.
    time.sleep(3.0)


def wait_for_mcu(mcu, expected_partition=None, timeout=20):
    """Wait until the updater answers after a reset; verify the booted image."""
    deadline = time.monotonic() + timeout
    while True:
        try:
            get_state_valid(mcu)
            break
        except UpdateError:
            if time.monotonic() >= deadline:
                return False
            time.sleep(0.5)
    if expected_partition is not None:
        # The boot status register is registered late in boot: retry until valid
        for _ in range(15):
            partition, boot_type = get_boot_status(mcu)
            if partition in ("A", "B"):
                return partition == expected_partition and boot_type == 0
            time.sleep(0.2)
        return False
    return True


def send_blocks(mcu, firmware, gap=0, gap_blocks=0, first_gap=0):
    """Mirror of run_update's block loop with configurable pacing."""
    for i, frame in enumerate(iter_blocks(firmware)):
        state, error = wait_for_state(mcu, UpdaterState.RUNNING, 5)
        if state != UpdaterState.RUNNING:
            raise UpdateError(f"block {i}: state={state.name} error={error.name}")
        if i == 0 and first_gap:
            time.sleep(first_gap)
        mcu.write_block(Reg.FW_DATA, frame)
        if gap and i < gap_blocks:
            time.sleep(gap)
    state, error = wait_for_state(mcu, UpdaterState.DONE, 10)
    if state != UpdaterState.DONE:
        raise UpdateError(f"finalize: state={state.name} error={error.name}")


import argparse


def case_t0_baseline(mcu, firmware, blocks, fw_crc):
    run_update(mcu, firmware, blocks)
    report("T0 baseline update", True)


def case_t1_untracked_reads(mcu, firmware, blocks, fw_crc):
    ok, note = True, ""
    for addr in (0x0000, 0x0010, 0x7777):
        try:
            mcu.read_register(addr)
        except UpdateError as exc:
            ok, note = False, str(exc)
    report("T1 untracked reads", ok, note)


def case_t2_garbage_idle(mcu, firmware, blocks, fw_crc):
    for _ in range(8):
        mcu.write_block(Reg.FW_DATA, b"\x5A" * 64)
        time.sleep(0.01)
    state, error = get_state_valid(mcu)
    report("T2 garbage while idle", state == UpdaterState.IDLE, f"state={state.name}")


def case_t3_start_abort(mcu, firmware, blocks, fw_crc):
    ensure_idle(mcu)
    start_update(mcu, firmware, blocks)
    ok, note = expect_state(mcu, UpdaterState.RUNNING)
    if ok:
        send_command(mcu, UpdaterCommand.ABORT)
        ok, note = expect_state(mcu, UpdaterState.IDLE)
    report("T3 start then abort", ok, note)


def case_t4_abort_midway(mcu, firmware, blocks, fw_crc):
    ensure_idle(mcu)
    start_update(mcu, firmware, blocks)
    ok, note = expect_state(mcu, UpdaterState.RUNNING)
    if ok:
        for i, frame in enumerate(iter_blocks(firmware)):
            mcu.write_block(Reg.FW_DATA, frame)
            if i == 2:
                break
        time.sleep(0.05)
        send_command(mcu, UpdaterCommand.ABORT)
        ok, note = expect_state(mcu, UpdaterState.IDLE)
    if ok:
        ensure_idle(mcu)
        try:
            run_update(mcu, firmware, blocks)
        except UpdateError as exc:
            ok, note = False, f"recovery update: {exc}"
    report("T4 abort mid-update then recover", ok, note)


def case_t5_truncated_frame(mcu, firmware, blocks, fw_crc):
    ensure_idle(mcu)
    start_update(mcu, firmware, blocks)
    ok, note = expect_state(mcu, UpdaterState.RUNNING)
    if ok:
        mcu.write_block(Reg.FW_DATA, next(iter_blocks(firmware))[:130])
        time.sleep(0.05)
        state, error = get_state_valid(mcu)
        note = f"interim state={state.name}"
        # Observed: a truncated frame wedges the updater in BUSY. It must
        # recover via ABORT and accept a clean update afterwards.
        send_command(mcu, UpdaterCommand.ABORT)
        abort_ok, abort_note = expect_state(mcu, UpdaterState.IDLE)
        ok = ok and abort_ok
        note += f"; abort: {abort_note}"
    if ok:
        try:
            run_update(mcu, firmware, blocks)
        except UpdateError as exc:
            ok, note = False, f"{note}; recovery update: {exc}"
    report("T5 truncated block frame", ok, note)


def case_t6_bad_crc(mcu, firmware, blocks, fw_crc):
    ensure_idle(mcu)
    start_update(mcu, firmware, blocks)
    ok, note = expect_state(mcu, UpdaterState.RUNNING)
    if ok:
        frame = bytearray(next(iter_blocks(firmware)))
        frame[-2] ^= 0xFF  # CRC field
        mcu.write_block(Reg.FW_DATA, bytes(frame))
        state, error = wait_for_state(mcu, UpdaterState.ERROR, 5)
        ok = state == UpdaterState.ERROR and error == UpdaterError.BLOCK_CRC_ERROR
        note = f"state={state.name} error={error.name}"
    ensure_idle(mcu)
    report("T6 corrupted block CRC", ok, note)


def case_t7_wrong_index(mcu, firmware, blocks, fw_crc):
    ensure_idle(mcu)
    start_update(mcu, firmware, blocks)
    ok, note = expect_state(mcu, UpdaterState.RUNNING)
    if ok:
        frame = bytearray(next(iter_blocks(firmware)))
        struct.pack_into("<H", frame, 256, 1)  # index 1, expected 0
        mcu.write_block(Reg.FW_DATA, bytes(frame))
        state, error = wait_for_state(mcu, UpdaterState.ERROR, 5)
        ok = state == UpdaterState.ERROR and error == UpdaterError.BLOCK_INDEX_ERROR
        note = f"state={state.name} error={error.name}"
    ensure_idle(mcu)
    report("T7 out-of-order block index", ok, note)


def case_t8_duplicate_start(mcu, firmware, blocks, fw_crc):
    ensure_idle(mcu)
    start_update(mcu, firmware, blocks)
    ok, note = expect_state(mcu, UpdaterState.RUNNING)
    if ok:
        send_command(mcu, UpdaterCommand.START)
        time.sleep(0.1)
        state, error = get_state_valid(mcu)
        ok = state == UpdaterState.RUNNING
        note = f"state={state.name}"
    if ok:
        try:
            send_blocks(mcu, firmware)
        except UpdateError as exc:
            ok, note = False, str(exc)
    report("T8 duplicate START ignored", ok, note)


def case_t9_garbage_during(mcu, firmware, blocks, fw_crc):
    ensure_idle(mcu)
    start_update(mcu, firmware, blocks)
    ok, note = expect_state(mcu, UpdaterState.RUNNING)
    if ok:
        mcu.write_block(Reg.FW_DATA, b"\x5A" * 64)
        time.sleep(0.05)
        state, error = get_state_valid(mcu)
        note = f"interim state={state.name}"
        send_command(mcu, UpdaterCommand.ABORT)
        abort_ok, abort_note = expect_state(mcu, UpdaterState.IDLE)
        ok = ok and abort_ok
        note += f"; abort: {abort_note}"
    if ok:
        try:
            run_update(mcu, firmware, blocks)
        except UpdateError as exc:
            ok, note = False, f"{note}; recovery update: {exc}"
    report("T9 garbage during update", ok, note)


def case_t10_timing(mcu, firmware, blocks, fw_crc):
    variants = (
        (0.01, 0, 0, "10ms gaps"),
        (1.0, 5, 0, "1s gaps for 5 blocks"),
        (0, 0, 4.5, "4.5s after first block"),
    )
    for gap, gap_blocks, first_gap, label in variants:
        ensure_idle(mcu)
        start_update(mcu, firmware, blocks)
        ok, note = expect_state(mcu, UpdaterState.RUNNING)
        if ok:
            try:
                send_blocks(mcu, firmware, gap=gap, gap_blocks=gap_blocks, first_gap=first_gap)
            except UpdateError as exc:
                ok, note = False, str(exc)
        report(f"T10 timing: {label}", ok, note)


def case_t11_killed(mcu, firmware, blocks, fw_crc):
    child = "/tmp/updater_stress_child.py"
    with open(child, "w") as f:
        f.write(CHILD)
    ensure_idle(mcu)
    subprocess.run(["python3", child, str(firmware_path), "5"])
    time.sleep(1.5)
    ok, note = True, ""
    ensure_idle(mcu)
    try:
        run_update(mcu, firmware, blocks)
    except UpdateError as exc:
        ok, note = False, str(exc)
    report("T11 killed mid-update then recover", ok, note)


def case_t12_final(mcu, firmware, blocks, fw_crc):
    ensure_idle(mcu)
    state, error = get_state_valid(mcu)
    fw_version = mcu.read_register(Reg.FW_VERSION)
    report("T12 final state", state == UpdaterState.IDLE, f"state={state.name} fw_version=0x{fw_version:04X}")


CASES = [
    ("T0", case_t0_baseline),
    ("T1", case_t1_untracked_reads),
    ("T2", case_t2_garbage_idle),
    ("T3", case_t3_start_abort),
    ("T4", case_t4_abort_midway),
    ("T5", case_t5_truncated_frame),
    ("T6", case_t6_bad_crc),
    ("T7", case_t7_wrong_index),
    ("T8", case_t8_duplicate_start),
    ("T9", case_t9_garbage_during),
    ("T10", case_t10_timing),
    ("T11", case_t11_killed),
    ("T12", case_t12_final),
]


def main():
    parser = argparse.ArgumentParser(description="MCU update-over-I2C stress tests")
    parser.add_argument("firmware", type=Path, help="firmware image to exercise")
    parser.add_argument("--case", help="run a single case by id, e.g. T5")
    parser.add_argument("--reset-between", action="store_true", help="reset the MCU after every case")
    args = parser.parse_args()

    global firmware_path
    firmware_path = args.firmware
    firmware, blocks, fw_crc = load_firmware(args.firmware)

    with McuI2C() as mcu:
        if not wait_for_mcu(mcu):
            print("MCU not responding at suite start")
            return 1
        baseline_partition, baseline_boot_type = get_boot_status(mcu)
        print(f"Baseline boot: partition={baseline_partition} boot_type={baseline_boot_type}")
        if baseline_partition not in ("A", "B") or baseline_boot_type != 0:
            print("Unexpected baseline boot status, aborting")
            return 1

        for case_id, fn in CASES:
            if args.case and case_id != args.case:
                continue
            try:
                ensure_idle(mcu)
                fn(mcu, firmware, blocks, fw_crc)
            except UpdateError as exc:
                report(case_id, False, str(exc))
            except Exception as exc:  # noqa: BLE001
                report(case_id, False, f"{type(exc).__name__}: {exc}")

            if args.reset_between:
                try:
                    reset_mcu(mcu)
                    ok = wait_for_mcu(mcu, baseline_partition)
                except UpdateError as exc:
                    ok = False
                    print(f"reset error: {exc}")
                partition, boot_type = get_boot_status(mcu)
                report(f"{case_id} reset", ok, f"partition={partition} boot_type={boot_type}")

    print(f"\n{stats['pass']} passed, {stats['fail']} failed")
    return 1 if stats["fail"] else 0


if __name__ == "__main__":
    sys.exit(main())
