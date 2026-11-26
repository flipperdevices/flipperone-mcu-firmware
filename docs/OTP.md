# OTP

What the firmware keeps in the RP2350 OTP: three blocks that describe the board and the device and sign both, and the USB white label the bootrom shows in BOOTSEL mode. The firmware side is `targets/f1/furi_hal/furi_hal_otp.{c,h}`; blocks are built off the device by `scripts/otp.py` and written with the `otp` CLI command.

OTP is write-once. A written block cannot be changed, only read.

## Map

The RP2350 OTP is 4096 rows of 24 bits, in 64 pages of 64 rows.

| Rows | Pages | What |
|---|---|---|
| `0x000..0x03F` | 0 | Factory: chip id, random id, calibration |
| `0x040..0x07F` | 1 | Boot configuration the bootrom reads. Ours in it: `BOOTSEL_LED_CFG` (`0x056`), `BOOT_FLAGS0.ENABLE_BOOTSEL_LED`, `USB_BOOT_FLAGS` (`0x059..0x05B`), `USB_WHITE_LABEL_ADDR` (`0x05C`) |
| `0x080..0x0BF` | 2 | Boot keys |
| `0x0C0..0x0D0` | 3 | Block 1, the board |
| `0x100..0x14F` | 4..5 | Block 2, the device |
| `0x180..0x1EA` | 6..7 | Block 3, the signatures |
| `0x400..0x467` | 16..17 | USB white label, written with block 1 |
| `0xF40..0xF7F` | 61 | Hardware keys |
| `0xF80..0xFFF` | 62..63 | Page lock rows |

Pages are locked whole: once a block is written, the rest of its pages is read only too. Everything else is free.

## Blocks

Every block is a run of ECC rows, 16 data bits each, little endian. It starts with a header and ends with a CRC:

| Field | Type | |
|---|---|---|
| `magic` | u32 | `0x544F3146`, "F1OT" |
| `index` | u16 | 1, 2 or 3 |
| `version` | u16 | Layout version of this block, 1 for all three |
| `timestamp` | u64 | When it was made, seconds since the epoch |
| ... | | The block's fields |
| `crc` | u16 | CRC-16/CCITT-FALSE of everything but `magic` and `crc` |

A block the firmware reads is one whose magic, index, version and CRC are right. ECC corrects one bit in a row and does not report a row it cannot correct; the CRC does.

### Block 1, the board (34 bytes)

| Field | Type | |
|---|---|---|
| `hw_version` | u16 | |
| `target` | u16 | `FW_TARGET` number: `f2` is 2 |
| `body` | u16 | |
| `connect` | u16 | |
| `chip_id` | u8[8] | RP2350 CHIPID (`rp2350_hardware_uid` in `device_info`). The device refuses a block 1 made for another chip |

A firmware runs on a board whose block 1 has its target, or on one with no block 1 (`furi_hal_version_do_i_belong_here()`).

### Block 2, the device (160 bytes)

| Field | Type | |
|---|---|---|
| `mac` | u8[8][6] | eth0, eth1, usb, wifi 2.4 GHz station, wifi 5 GHz station, wifi 2.4 GHz AP, wifi 5 GHz AP, bt |
| `name` | char[32] | NUL terminated |
| `model` | char[32] | NUL terminated |
| `color` | u16 | |
| `region` | char[2] | Two letters, no NUL |
| `cpu_id` | u8[16] | |
| `ram_gb` | u32 | |
| `ufs_gb` | u32 | |
| `display` | u16 | |

### Block 3, the signatures (214 bytes)

| Field | Type | |
|---|---|---|
| `key_id` | u16 | Key in `targets/f1/furi_hal/furi_hal_otp_keys.h` |
| `curve` | u16 | 1: ECDSA P-256 with SHA-256 |
| `public_key` | u8[64] | X ‖ Y. Must equal the firmware's key `key_id` |
| `block1_signature` | u8[64] | r ‖ s over `chip_id ‖ "-otp1-" ‖ block 1` |
| `block2_signature` | u8[64] | r ‖ s over `chip_id ‖ "-otp2-" ‖ block 2` |

The block carries its key so that `otp.py verify` can check a set of files on its own. The firmware trusts the key only as one it carries itself: anyone can write a key and signatures of their own. With the chip id in the signed data, the signatures belong to one chip, so a copy of another device's OTP does not verify.

`rp2350_otp_valid` in `device_info` is true when all three blocks are written and the signatures verify.

## Writing

Blocks are written in order: 1, then 2, then 3. The device refuses a block whose earlier blocks are not written, a block 1 for another chip, and a block 3 whose key or signatures it does not accept.

A block is written in this order:

1. Every row but the magic.
2. Those rows are read back. The bootrom does not check what it wrote, so the magic only goes on a block that reads back right.
3. The magic.
4. The block's pages are locked, read only for Secure, Non-secure and bootloader access. The lock is permanent from the next reset; until then the firmware's software lock holds it.

Block 1 also writes the USB white label (below), with the board id `F<target>B<body>C<connect>`, and locks its pages.

A write cut short (power lost, a reset) is finished by sending the same block again, byte for byte. Each row is read raw to tell what it holds:

- the wanted value, as is or inverted (the bootrom's bit repair by polarity): done;
- blank, or a part of the wanted value's bits: a write cut short, written again;
- anything else, including a whole ECC row of another value: other data. The block is refused before anything is written.

A block made anew differs from the one cut short, if only by its timestamp, so `otp.py` keeps the files it makes and never overwrites one.

A page lock row has three copies of the Secure, Non-secure and bootloader locks, each bit taken by two of three. The row is read only to the access its own page's lock denies, so the Non-secure and bootloader bits are written first and the Secure ones last: a lock cut short can always be finished by this Secure firmware.

## USB white label

What the bootrom shows over USB in BOOTSEL mode: VID `0x37C1`, PID `0xF102`, manufacturer, product, the mass storage volume label and SCSI strings, the update page link, model and board id. The table and its strings live at `0x400`, `USB_WHITE_LABEL_ADDR` points to it and `USB_BOOT_FLAGS` marks each entry valid.

Block 1 writes it after the block itself. Before writing block 1, every row of the white label is checked as above, so a white label that holds other data refuses block 1 with nothing written.

`rp2350_otp_white_label` in `device_info` is true when the bootrom takes the white label: each flag of `USB_BOOT_FLAGS` by two of its three copies. It is not part of `rp2350_otp_valid`.

## Tools

`scripts/otp.py` needs the `cryptography` package, as pinned in `assets/python/requirements.txt`.

```sh
otp.py generate-key -o key.pem --public key.pub
otp.py create-otp1 --hw-version 1 --target 2 --body 1 --connect 1 --chip-id <rp2350_hardware_uid> -o otp1.bin
otp.py create-otp2 --eth0 ... --name ... -o otp2.bin
otp.py create-otp3 --key key.pem --key-id 0 --otp1 otp1.bin --otp2 otp2.bin -o otp3.bin
otp.py verify --otp1 otp1.bin --otp2 otp2.bin --otp3 otp3.bin
otp.py load otp1.bin
```

- `generate-key` makes a P-256 key pair. The private key file is created readable by its owner only. The public half goes into `furi_hal_otp_keys.h` under a new key id.
- `create-otp3` refuses a key that is not the firmware's key `--key-id`.
- `verify` checks both signatures with the key in block 3, and that the firmware carries that key.
- `load` prints a block file and its hex.

On the device:

- `otp program OTP<n> <hex>` writes a block. It asks for confirmation; `--answer y` of `scripts/agent_tools/f1.py cli` gives it.
- `otp dump OTP<n>` prints a block's rows as they are, valid or not.
- `device_info` prints the blocks, decoded.
