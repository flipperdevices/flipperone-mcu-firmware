#pragma once

#include <furi.h>

#define RECORD_UPDATER "updater"

// Keep in sync with Linux updater code
typedef enum {
    UpdaterStateIdle = 0,
    UpdaterStateRunning,
    UpdaterStateBusy,
    UpdaterStateDone,
    UpdaterStateError,
} UpdaterState;

typedef enum {
    UpdaterErrorNone = 0,
    UpdaterErrorNotStarted,
    UpdaterErrorSizeError,
    UpdaterErrorBlockCrcError,
    UpdaterErrorBlockIndexError,
    UpdaterErrorFwCrcError,
    UpdaterErrorFwVersionError,
    UpdaterErrorProtocolError, // I2C queue overflow or out-of-order data write
    UpdaterErrorFwWriteError,
} UpdaterError;

typedef enum {
    UpdaterCommandNone = 0,
    UpdaterCommandStart,
    UpdaterCommandAbort,
} UpdaterCommand;

typedef void (*UpdaterStateCallback)(UpdaterState state, UpdaterError error, void* context);

typedef struct Updater Updater;

uint16_t updater_crc16(const uint8_t* buf, size_t len, uint16_t crc_init);

uint16_t updater_get_current_fw_version(Updater* updater);

void updater_set_state_callback(Updater* updater, UpdaterStateCallback callback, void* context);

// I2C intercom callbacks, registered by i2c_negotiator; called from the I2C slave ISR.
// Definitions carry the __isr/__not_in_flash_func attributes.
bool updater_on_i2c_data_write(void* context, uint16_t offset, uint8_t value);

void updater_on_i2c_crc_reg_write(void* context, uint16_t address, uint16_t value);

void updater_on_i2c_blocks_reg_write(void* context, uint16_t address, uint16_t value);

void updater_on_i2c_command_reg_write(void* context, uint16_t address, uint16_t value);
