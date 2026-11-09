#pragma once

#include <furi.h>

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
 * @brief Callback for updater state changes.
 *
 * @param state New updater state.
 * @param error Associated error; UpdaterErrorNone unless the state is UpdaterStateError.
 * @param context Callback context passed at registration.
 */
typedef void (*UpdaterStateCallback)(UpdaterState state, UpdaterError error, void* context);

/** Updater instance. */
typedef struct Updater Updater;

/**
 * @brief Compute the updater CRC-16 over a buffer.
 *
 * @param buf Buffer to hash.
 * @param len Buffer length in bytes.
 * @param crc_init Initial CRC value; 0xFFFF for a fresh calculation, or a
 *                 previous result to chain multiple buffers.
 * @return CRC-16 value.
 */
uint16_t updater_crc16(const uint8_t* buf, size_t len, uint16_t crc_init);

/**
 * @brief Get the currently installed firmware version.
 *
 * @param updater Updater instance.
 * @return Firmware version.
 */
uint16_t updater_get_current_fw_version(Updater* updater);

/**
 * @brief Set the state change callback.
 *
 * @param updater Updater instance.
 * @param callback Callback to invoke on state changes, may be NULL.
 * @param context Passed to the callback.
 */
void updater_set_state_callback(Updater* updater, UpdaterStateCallback callback, void* context);

/**
 * @brief Serve a host write into the updater data region.
 *
 * @param context Updater instance.
 * @param offset Byte offset relative to the data region base.
 * @param value Byte written by the host.
 * @return true if the byte was accepted, false otherwise.
 *
 * @warning I2C intercom callback, registered by i2c_negotiator and called
 *          from the I2C slave ISR; the definition carries
 *          __isr/__not_in_flash_func.
 */
bool updater_on_i2c_data_write(void* context, uint16_t offset, uint8_t value);

/**
 * @brief Serve a host write to the update CRC register.
 *
 * @param context Updater instance.
 * @param address Register address, always even.
 * @param value Full 16-bit register value.
 *
 * @warning Same ISR context as updater_on_i2c_data_write.
 */
void updater_on_i2c_crc_reg_write(void* context, uint16_t address, uint16_t value);

/**
 * @brief Serve a host write to the blocks-written register.
 *
 * @param context Updater instance.
 * @param address Register address, always even.
 * @param value Full 16-bit register value.
 *
 * @warning Same ISR context as updater_on_i2c_data_write.
 */
void updater_on_i2c_blocks_reg_write(void* context, uint16_t address, uint16_t value);

/**
 * @brief Serve a host write to the command register.
 *
 * @param context Updater instance.
 * @param address Register address, always even.
 * @param value Full 16-bit register value.
 *
 * @warning Same ISR context as updater_on_i2c_data_write.
 */
void updater_on_i2c_command_reg_write(void* context, uint16_t address, uint16_t value);
