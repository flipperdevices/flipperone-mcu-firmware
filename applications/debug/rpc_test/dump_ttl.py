import serial, time, sys

PORT = sys.argv[1] if len(sys.argv) > 1 else "COM41"

ser = serial.Serial(PORT, 1500000, timeout=1.0)
ser.reset_input_buffer()
ser.reset_output_buffer()

ser.write(b'rpc\r')
ser.flush()

all_data = b''
ser.timeout = 0.3
deadline = time.monotonic() + 3.0
while time.monotonic() < deadline:
    chunk = ser.read(4096)
    if chunk:
        all_data += chunk

ser.close()

fd_pos = all_data.find(b'\xfd')
print(f'Total received: {len(all_data)} bytes')
print()

if fd_pos >= 0:
    before = all_data[:fd_pos]
    after = all_data[fd_pos+1:]
    print(f'--- BEFORE 0xFD ({len(before)} bytes) ---')
    print(f'  STR: {before}')
    print(f'  HEX: {before.hex()}')
    print()
    print(f'*** 0xFD MARKER at offset {fd_pos} ***')
    print()
    print(f'--- AFTER 0xFD ({len(after)} bytes) ---')
    print(f'  STR: {after}')
    print(f'  HEX: {after.hex()}')
else:
    print('0xFD NOT FOUND!')
    print(f'  STR: {all_data}')
    print(f'  HEX: {all_data.hex()}')
