# Flipper One MCU, target f2. Identical to f100 until its hardware diverges.
#
# To replace an f100 file, drop it into targets/f2/ under the same relative path —
# the copy here wins, no declaration needed. fw_target_remove() is only for f100
# files that f2 does not have at all; fw_target_sources() only for paths f100 has
# no equivalent of.

fw_base(f100)
