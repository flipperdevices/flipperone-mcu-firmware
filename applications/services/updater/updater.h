#pragma once

#include <furi.h>
#include <furi_hal_flash.h>

#define RECORD_UPDATER "updater"

/** Updater block size in bytes; must match I2C_UPDATER_BLOCK_SIZE in i2c_registers_map.h. */
#define UPDATER_BLOCK_SIZE (256)

/**
 * @brief Updater state.
 *
 * Keep in sync with the Linux updater code.
 */
typedef enum {
    UpdaterStateIdle = 0,
    UpdaterStateRunning,
    UpdaterStateBusy,
    UpdaterStateDone,
    UpdaterStateError,
} UpdaterState;

/**
 * @brief Updater error codes.
 *
 * Keep in sync with the Linux updater code.
 */
typedef enum {
    UpdaterErrorNone = 0,
    UpdaterErrorNotStarted,
    UpdaterErrorSizeError,
    UpdaterErrorBlockCrcError,
    UpdaterErrorBlockIndexError,
    UpdaterErrorFwCrcError,
    UpdaterErrorFwVersionError,
    UpdaterErrorProtocolError, /**< I2C queue overflow or out-of-order data write */
    UpdaterErrorFwWriteError,
    UpdaterErrorFwInvalidateError,
    UpdaterErrorFwParamsError,
} UpdaterError;

/**
 * @brief Updater commands.
 *
 * Keep in sync with the Linux updater code.
 */
typedef enum {
    UpdaterCommandNone = 0,
    UpdaterCommandStart,
    UpdaterCommandAbort,
} UpdaterCommand;

/**
 * @brief How this boot relates to an update. Wire values of the boot type
 *        byte in the boot status register.
 */
typedef enum {
    UpdaterBootTypeNormal = 0, /**< Not after an update */
    UpdaterBootTypeFlashUpdate = 1, /**< Switched to a new image, bought at startup */
    UpdaterBootTypeReverted = 2, /**< A new image failed before its buy: the old one runs */
} UpdaterBootType;

/** Updater instance. */
typedef struct Updater Updater;

/**
 * @brief Compute the updater CRC-16 over a buffer.
 *
 * @param buf Buffer to hash.
 * @param len Buffer length in bytes.
 * @param crc_init Initial CRC value; 0x0000 for a fresh calculation, or a
 *                 previous result to chain multiple buffers.
 * @return CRC-16 value.
 */
uint16_t updater_crc16(const uint8_t* buf, size_t len, uint16_t crc_init);

/**
 * @brief Register the updater's I2C registers (0xF000 block). Once, before
 *        i2c_intercom_setup_end(); the updater keeps their values current.
 *
 * @param updater Updater instance.
 */
void updater_i2c_setup(Updater* updater);

/**
 * @brief Whether the last update staged an image that can be booted.
 *
 * @param updater Updater instance.
 * @return true if updater_activate() would switch to it.
 */
bool updater_has_staged_image(Updater* updater);

/**
 * @brief Boot the image the last update staged, as a flash-update boot.
 *
 * Returns only when nothing is staged, or the slot holds no valid image.
 *
 * @param updater Updater instance.
 */
void updater_activate(Updater* updater);

/**
 * @brief How this boot relates to an update.
 *
 * @param updater Updater instance.
 * @return Boot type.
 */
UpdaterBootType updater_get_boot_type(Updater* updater);
