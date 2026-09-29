adapter speed 5000

init
reset halt

set _FW_A_offset 0x10002000
set _FW_B_offset 0x10782000

# Paths below are relative to the directory openocd is started from. The Makefile
# passes its build directory with -c "set BUILD_DIR ..." ahead of this script.
if { ![info exists BUILD_DIR] } { set BUILD_DIR build }

# Flash partition table at 0x0
echo "Flashing partition table..."
flash write_image erase $BUILD_DIR/partition_table.bin 0x10000000
verify_image $BUILD_DIR/partition_table.bin 0x10000000

# Flash current development binary at partition A (8k offset)
echo "Flashing firmware to partition A..."
flash write_image erase $BUILD_DIR/flipperone-mcu-firmware.bin $_FW_A_offset
verify_image $BUILD_DIR/flipperone-mcu-firmware.bin $_FW_A_offset

# Erase first sector of partition B (invalidate any old firmware)
echo "Erasing partition B header..."
flash erase_address $_FW_B_offset 0x1000

# Reset and run
echo "Flashing complete, rebooting..."
reset run
shutdown