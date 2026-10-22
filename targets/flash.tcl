adapter speed 5000

init
reset halt

set _FW_A_offset 0x10002000
set _FW_B_offset 0x10782000

# Flash partition table at 0x0
echo "Flashing partition table..."
flash write_image erase build/partition_table.bin 0x10000000
verify_image build/partition_table.bin 0x10000000

# Flash current development binary at partition A (8k offset)
echo "Flashing firmware to partition A..."
flash write_image erase build/flipperone-mcu-firmware.bin $_FW_A_offset
verify_image build/flipperone-mcu-firmware.bin $_FW_A_offset

# Erase first sector of partition B (invalidate any old firmware)
echo "Erasing partition B header..."
flash erase_address $_FW_B_offset 0x1000

# Reset and run
echo "Flashing complete, rebooting..."
reset run
shutdown