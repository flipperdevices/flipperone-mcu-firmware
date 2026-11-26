#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* OTP blocks 1..3, the USB white label and the page locks: see OTP.md */

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
    uint64_t timestamp; /* seconds since the epoch */
} FuriHalOtpHeader;

#define FURI_HAL_OTP_CHIP_ID_SIZE 8 /* RP2350 CHIPID, as pico_get_unique_board_id() */
#define FURI_HAL_OTP_MAC_SIZE     6
#define FURI_HAL_OTP_STR_SIZE     32 /* 31 chars + NUL */
#define FURI_HAL_OTP_PUBLIC_SIZE  64 /* X || Y */
#define FURI_HAL_OTP_SIGN_SIZE    64 /* r || s */

typedef enum {
    FuriHalOtpCurveSecp256r1 = 1, /* ECDSA P-256 with SHA-256 */
} FuriHalOtpCurve;

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
    uint16_t key_id; /* must be a key the firmware carries */
    uint16_t curve; /* FuriHalOtpCurve */
    uint8_t public_key[FURI_HAL_OTP_PUBLIC_SIZE]; /* must equal the firmware's key_id */
    uint8_t block1_signature[FURI_HAL_OTP_SIGN_SIZE];
    uint8_t block2_signature[FURI_HAL_OTP_SIGN_SIZE];
    uint16_t crc;
} FuriHalOtpBlock3Data;

#define FURI_HAL_OTP_BLOCK_MAX_SIZE sizeof(FuriHalOtpBlock3Data)

void furi_hal_otp_init(void);

typedef enum {
    FuriHalOtpErrorNone,
    FuriHalOtpErrorInvalidBlock, /* Wrong size, magic, index, version or CRC */
    FuriHalOtpErrorOrder, /* An earlier block is not written */
    FuriHalOtpErrorChipId, /* Block 1 is made for another chip */
    FuriHalOtpErrorSignature, /* Block 3: unknown key or curve, or a signature that does not verify */
    FuriHalOtpErrorAlreadyWritten, /* The block is written with other data */
    FuriHalOtpErrorNotBlank, /* No magic, and rows hold other data than this write's */
    FuriHalOtpErrorWriteFailed,
    FuriHalOtpErrorLockFailed, /* Written, but its pages are not locked */
    FuriHalOtpErrorWhiteLabelFailed, /* Block 1 written, the USB white label not */
    FuriHalOtpErrorWhiteLabelMismatch, /* The white label holds other data: nothing written */
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

/** Whether the bootrom takes the USB white label as written. Block 1 writes
 * it; it is not part of furi_hal_otp_valid(). */
bool furi_hal_otp_usb_white_label_valid(void);

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
