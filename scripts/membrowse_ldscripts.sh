#!/bin/bash
# Flattens the linker script tree into <build dir>/ldscripts for MemBrowse.
#
# pico-sdk 2.3+ splits the linker script into memmap_default.ld plus a tree of
# INCLUDE'd .incl fragments (see the tree comment inside memmap_default.ld), and
# MemBrowse resolves INCLUDE relative to the including file, so the whole tree has
# to end up in one directory. Order matters: SDK defaults first, then generated
# fragments, then our restricted FLASH region on top.
#
# Shared by build.yml and membrowse-onboard.yml. The onboard workflow runs it
# against historical commits, so it must not assume the current repository layout.
#
# Usage: membrowse_ldscripts.sh <repo dir> <build dir> <pico-sdk dir>
set -euo pipefail

REPO=$1
BUILD=$2
SDK=$3
OUT=$BUILD/ldscripts

mkdir -p "$OUT"
cp "$SDK/src/rp2350/pico_platform/memmap_default.ld" "$OUT/"
cp "$SDK"/src/rp2_common/pico_standard_link/script_include/*.incl "$OUT/"
cp "$SDK"/src/rp2350/pico_platform/script_include/*.incl "$OUT/"

# Per-target fragments the SDK generates at configure, if there are any
cp "$BUILD"/generated/pico_standard_link/*/*.incl "$OUT/" 2>/dev/null || true

# Region files the SDK writes into the build tree. The SDK's memory_flash.incl
# INCLUDEs pico_flash_region.ld; commits before targets/memory_flash.incl existed
# restricted FLASH by overwriting that file at configure, so it must come along.
cp "$BUILD"/pico-sdk/src/rp2_common/pico_standard_link/pico_psram_region.ld "$OUT/"
cp "$BUILD"/pico-sdk/src/rp2_common/pico_standard_link/pico_flash_region.ld "$OUT/"

# Our restricted FLASH region. The build picks it up over the SDK's copy through
# pico_add_linker_script_override_path(); here it simply replaces the SDK file.
if [ -f "$REPO/targets/memory_flash.incl" ]; then
    cp "$REPO/targets/memory_flash.incl" "$OUT/"
fi
