#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * OTP map (RP2350: 4096 rows of 24 bits, 64 pages of 64 rows)
 *
 * Rows           Pages   Owner
 * 0x000..0x03F   0       Factory: chip id, random id, calibration
 * 0x040..0x07F   1       Boot config read by the bootrom. Ours in it:
 *                          BOOTSEL_LED_CFG (0x056) and BOOT_FLAGS0.ENABLE_BOOTSEL_LED,
 *                          USB_BOOT_FLAGS (0x059..0x05B), USB_WHITE_LABEL_ADDR (0x05C)
 * 0x080..0x0BF   2       Boot keys
 * 0x0C0..0x0D0   3       Block 1, board
 * 0x100..0x14F   4..5    Block 2, device
 * 0x180..0x1C9   6..7    Block 3, signatures
 * 0x400..0x467   16..17  USB white label, written with block 1
 * 0xF40..0xF7F   61      Hardware keys
 * 0xF80..0xFFF   62..63  Page lock rows
 * Pages are locked whole, so the rest of pages 3..7 and 16..17 (0x0D1..0x0FF,
 * 0x150..0x17F, 0x1CA..0x1FF, 0x468..0x47F) is read only once their block
 * is written. Everything else is free.
 *
 * Blocks are ECC rows of 16 data bits, little endian, built off the device
 * (scripts/otp.py) and written whole, in order: block 1, then 2, then 3. Each
 * starts with FuriHalOtpHeader and ends with a CRC-16/CCITT-FALSE of all its
 * rows but the magic and the CRC. The magic is written last, after the rest
 * reads back right, so a block cut short is never taken as written. A
 * written block's pages are locked: read only for Secure and Non-secure
 * access, which the hardware applies from the next reset (until then SW_LOCKn
 * holds it), and for the USB bootloader, which honours LOCK_BL.
 *
 * Block 3 holds ECDSA P-256 signatures (SHA-256, r || s) of
 *   chip id || "-otp1-" || block 1   and   chip id || "-otp2-" || block 2
 * by a production key whose public half the firmware carries, by key id.
 */

#define FURI_HAL_OTP_MAGIC 0x544F3146 /* "F1OT" */

typedef enum {
    FuriHalOtpBlock1 = 1,
    FuriHalOtpBlock2,
    FuriHalOtpBlock3,
    FuriHalOtpBlockCount = FuriHalOtpBlock3,
} FuriHalOtpBlock;

typedef struct __attribute__((packed)) {
    uint32_t magic; /* FURI_HAL_OTP_MAGIC */
    uint16_t index; /* FuriHalOtpBlock */
    uint16_t version; /* layout version of this block */
    uint64_t timestamp; /* when it was made, seconds since the epoch */
} FuriHalOtpHeader;

#define FURI_HAL_OTP_CHIP_ID_SIZE 8 /* RP2350 CHIPID, as pico_get_unique_board_id() */
#define FURI_HAL_OTP_MAC_SIZE     6
#define FURI_HAL_OTP_STR_SIZE     32 /* 31 chars + NUL */
#define FURI_HAL_OTP_SIGN_SIZE    64 /* ECDSA P-256 r || s */

/* Block 1, version 1: the board */
typedef struct __attribute__((packed)) {
    FuriHalOtpHeader header;
    uint16_t hw_version;
    uint16_t target; /* FW_TARGET number: f2 is 2 */
    uint16_t body;
    uint16_t connect;
    uint8_t chip_id[FURI_HAL_OTP_CHIP_ID_SIZE]; /* must be this chip's */
    uint16_t crc;
} FuriHalOtpBlock1Data;

typedef enum {
    FuriHalOtpMacEth0,
    FuriHalOtpMacEth1,
    FuriHalOtpMacUsb,
    FuriHalOtpMacWifi24Sta,
    FuriHalOtpMacWifi5Sta,
    FuriHalOtpMacWifi24Ap,
    FuriHalOtpMacWifi5Ap,
    FuriHalOtpMacBt,
    FuriHalOtpMacCount,
} FuriHalOtpMac;

/* Block 2, version 1: the device */
typedef struct __attribute__((packed)) {
    FuriHalOtpHeader header;
    uint8_t mac[FuriHalOtpMacCount][FURI_HAL_OTP_MAC_SIZE];
    char name[FURI_HAL_OTP_STR_SIZE]; /* NUL terminated */
    char model[FURI_HAL_OTP_STR_SIZE]; /* NUL terminated */
    uint16_t color;
    char region[2]; /* two letters, no NUL */
    uint8_t cpu_id[16];
    uint32_t ram_gb;
    uint32_t ufs_gb;
    uint16_t display;
    uint16_t crc;
} FuriHalOtpBlock2Data;

/* Block 3, version 1: the signatures */
typedef struct __attribute__((packed)) {
    FuriHalOtpHeader header;
    uint16_t key_id;
    uint8_t block1_signature[FURI_HAL_OTP_SIGN_SIZE];
    uint8_t block2_signature[FURI_HAL_OTP_SIGN_SIZE];
    uint16_t crc;
} FuriHalOtpBlock3Data;

#define FURI_HAL_OTP_BLOCK_MAX_SIZE sizeof(FuriHalOtpBlock2Data)

void furi_hal_otp_init(void);

typedef enum {
    FuriHalOtpErrorNone,
    FuriHalOtpErrorInvalidBlock, /* Wrong size, magic, index, version or CRC */
    FuriHalOtpErrorOrder, /* An earlier block is not written */
    FuriHalOtpErrorChipId, /* Block 1 is made for another chip */
    FuriHalOtpErrorSignature, /* Block 3: unknown key or a signature that does not verify */
    FuriHalOtpErrorAlreadyWritten, /* The block is written with other data */
    FuriHalOtpErrorNotBlank, /* No magic, and rows hold other data than this write's */
    FuriHalOtpErrorWriteFailed,
    FuriHalOtpErrorLockFailed, /* Written, but its pages are not locked */
    FuriHalOtpErrorWhiteLabelFailed, /* Block 1 written, the USB white label not */
    FuriHalOtpErrorWhiteLabelMismatch, /* The white label has another board id: nothing written */
} FuriHalOtpError;

const char* furi_hal_otp_error_to_string(FuriHalOtpError error);

/** Size of a block in bytes, 0 for an unknown index */
size_t furi_hal_otp_block_size(FuriHalOtpBlock block);

/** Read a block: true if it is written whole, of a known version, with a
 * right CRC. data is zeroed otherwise. size must be the block's size. */
bool furi_hal_otp_block_read(FuriHalOtpBlock block, void* data, size_t size);

/** Read a block's rows as they are, valid or not, for a dump */
bool furi_hal_otp_block_dump(FuriHalOtpBlock block, void* data, size_t size);

/** Write a block built off the device and lock its pages. Irreversible.
 * Refused unless it is valid itself, the blocks before it are written, block 1
 * carries this chip's id and block 3's signatures verify. Block 1 also writes
 * the USB white label with its board id "F<target>B<body>C<connect>". A call
 * cut short is finished by calling again with the same block. */
FuriHalOtpError furi_hal_otp_block_program(FuriHalOtpBlock block, const void* data, size_t size);

/** Whether all three blocks are written and block 3's signatures verify with
 * a key the firmware carries */
bool furi_hal_otp_valid(void);

/** Check if BOOTSEL LED is enabled in OTP (BOOT_FLAGS0.ENABLE_BOOTSEL_LED)
 *
 * @return true if enabled
 */
bool furi_hal_otp_bootsel_led_valid(void);

/** Enable BOOTSEL LED: writes BOOTSEL_LED_CFG (GPIO 35, active high)
 * and sets BOOT_FLAGS0.ENABLE_BOOTSEL_LED. OTP write is irreversible.
 *
 * @return true on success
 */
bool furi_hal_otp_write_bootsel_led(void);
