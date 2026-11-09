#include "updater.h"
#include <furi_hal_flash.h>
#include <hardware/flash.h>

#define TAG "Updater"

#define UPDATER_BLOCK_SIZE        (256)
#define UPDATER_BLOCKS_PER_SECTOR (FLASH_SECTOR_SIZE / UPDATER_BLOCK_SIZE)
#define UPDATER_MIN_BLOCKS        (4 * UPDATER_BLOCKS_PER_SECTOR)

// ISR-reported protocol errors, promoted to UpdaterStateError by the event loop
#define UPDATER_ISR_ERROR_QUEUE_FULL   (1 << 0)
#define UPDATER_ISR_ERROR_OUT_OF_ORDER (1 << 1)

#define UPDATER_CUSTOM_EVENT_ERROR (1 << 0)

typedef union {
    struct {
        uint16_t version;
        uint16_t counter;
    };
    uint16_t raw[2];
} ImageVersion;

typedef struct {
    uint16_t crc;
    uint16_t blocks;
    bool crc_set;
    bool size_set;
} UpdateParams;

typedef struct {
    union {
        struct {
            uint8_t data[UPDATER_BLOCK_SIZE];
            uint16_t block_idx;
            uint16_t block_crc;
        };
        uint8_t raw[UPDATER_BLOCK_SIZE + 4];
    };
    size_t offset;
} UpdaterI2cRx;

struct Updater {
    FuriEventLoop* event_loop;
    FuriMessageQueue* message_queue;
    UpdaterState state;

    UpdateParams update_params;
    uint16_t current_block;

    FlashPartitionId active_partition;
    ImageVersion current_version;

    size_t inactive_partition_address;
    size_t inactive_partition_size;
    uint8_t first_sector_buf[FLASH_SECTOR_SIZE];

    // Set by the I2C ISR callbacks, read and cleared by the event loop thread
    volatile uint32_t isr_error;
    bool update_supported;

    UpdaterStateCallback state_callback;
    void* state_callback_context;

    UpdaterI2cRx i2c_rx;
};
static_assert(sizeof(((struct Updater*)0)->i2c_rx.raw) == (sizeof(((struct Updater*)0)->i2c_rx.data) + 4));

typedef struct {
    enum {
        UpdaterMessageSetCrc,
        UpdaterMessageSetSize,
        UpdaterMessageStartUpdate,
        UpdaterMessageAbortUpdate,
        UpdaterMessageWriteBlockStart,
        UpdaterMessageWriteBlockDone,
    } type;
    union {
        uint16_t fw_crc;
        uint16_t fw_blocks;
        struct {
            uint16_t block_idx;
            uint16_t block_crc;
            uint8_t data[UPDATER_BLOCK_SIZE];
        } block;
    };
} UpdaterMessage;

// ISR queue put: overflow records an error instead of crashing
static void __isr __not_in_flash_func(updater_message_queue_put_from_isr)(Updater* updater, const UpdaterMessage* msg) {
    if(furi_message_queue_put(updater->message_queue, msg, 0) == FuriStatusOk) return;

    updater->isr_error |= UPDATER_ISR_ERROR_QUEUE_FULL;
    furi_event_loop_set_custom_event(updater->event_loop, UPDATER_CUSTOM_EVENT_ERROR);
}

// I2C write callbacks, called from interrupt context
void __isr __not_in_flash_func(updater_on_i2c_crc_reg_write)(void* context, uint16_t address, uint16_t value) {
    UNUSED(address);
    Updater* updater = context;
    furi_check(updater);

    const UpdaterMessage msg = {
        .type = UpdaterMessageSetCrc,
        .fw_crc = value,
    };
    updater_message_queue_put_from_isr(updater, &msg);
}

void __isr __not_in_flash_func(updater_on_i2c_blocks_reg_write)(void* context, uint16_t address, uint16_t value) {
    UNUSED(address);
    Updater* updater = context;
    furi_check(updater);

    const UpdaterMessage msg = {
        .type = UpdaterMessageSetSize,
        .fw_blocks = value,
    };
    updater_message_queue_put_from_isr(updater, &msg);
}

void __isr __not_in_flash_func(updater_on_i2c_command_reg_write)(void* context, uint16_t address, uint16_t value) {
    UNUSED(address);
    Updater* updater = context;
    furi_check(updater);

    // Only exact command values are meaningful (guards against bus mis-sync)
    if(value != UpdaterCommandStart && value != UpdaterCommandAbort) return;

    const UpdaterMessage msg = {
        .type = (value == UpdaterCommandStart) ? UpdaterMessageStartUpdate : UpdaterMessageAbortUpdate,
    };
    updater_message_queue_put_from_isr(updater, &msg);
}

bool __isr __not_in_flash_func(updater_on_i2c_data_write)(void* context, uint16_t offset, uint8_t value) {
    Updater* updater = context;
    furi_check(updater);

    if(offset >= sizeof(updater->i2c_rx.raw)) return false;

    if(offset == 0) {
        memset(updater->i2c_rx.raw, 0, sizeof(updater->i2c_rx.raw));
        updater->i2c_rx.offset = 0;
        const UpdaterMessage msg = {.type = UpdaterMessageWriteBlockStart};
        updater_message_queue_put_from_isr(updater, &msg);
    }

    if(offset != updater->i2c_rx.offset) {
        // Out-of-order: record the error and drop the rest of the block
        updater->isr_error |= UPDATER_ISR_ERROR_OUT_OF_ORDER;
        furi_event_loop_set_custom_event(updater->event_loop, UPDATER_CUSTOM_EVENT_ERROR);
        updater->i2c_rx.offset = 0;
        return true;
    }

    updater->i2c_rx.raw[updater->i2c_rx.offset++] = value;

    if(updater->i2c_rx.offset == sizeof(updater->i2c_rx.raw)) {
        UpdaterMessage msg = {
            .type = UpdaterMessageWriteBlockDone,
            .block.block_idx = updater->i2c_rx.block_idx,
            .block.block_crc = updater->i2c_rx.block_crc,
        };
        memcpy(msg.block.data, updater->i2c_rx.data, sizeof(msg.block.data));
        updater_message_queue_put_from_isr(updater, &msg);
    }

    return true;
}

static void updater_state_change(Updater* updater, UpdaterState new_state, UpdaterError error) {
    furi_check(updater);
    if(updater->state != new_state) {
        updater->state = new_state;
        if(updater->state_callback) {
            updater->state_callback(new_state, new_state == UpdaterStateError ? error : UpdaterErrorNone, updater->state_callback_context);
        }
    }
}

static void updater_finalize_update(Updater* updater) {
    ImageVersion new_version;
    bool success = furi_hal_flash_get_image_version(updater->first_sector_buf, new_version.raw);
    if(!success) {
        FURI_LOG_E(TAG, "Update image missing version information");
        updater_state_change(updater, UpdaterStateError, UpdaterErrorFwVersionError);
        return;
    }

    bool do_downgrade = false;

    if(new_version.version < updater->current_version.version) {
        do_downgrade = true;
        new_version.counter = 0;
    } else if(new_version.version > updater->current_version.version) {
        new_version.counter = 0;
    } else if(new_version.version == updater->current_version.version) {
        if(updater->current_version.counter == 0xFFFF) {
            new_version.counter = 0;
            do_downgrade = true;
        } else {
            new_version.counter = updater->current_version.counter + 1;
        }
    }

    FURI_LOG_I(
        TAG,
        "ver: %04X->%04X, cnt: %u->%u, downgrade: %s",
        updater->current_version.version,
        new_version.version,
        updater->current_version.counter,
        new_version.counter,
        do_downgrade ? "YES" : "no");

    furi_check(furi_hal_flash_set_image_version(updater->first_sector_buf, new_version.raw));

    if(!furi_hal_flash_write(updater->inactive_partition_address, updater->first_sector_buf, FLASH_SECTOR_SIZE)) {
        updater_state_change(updater, UpdaterStateError, UpdaterErrorFwWriteError);
        return;
    }

    if(do_downgrade) {
        furi_hal_flash_fw_partition_invalidate(updater->active_partition);
    }

    updater_state_change(updater, UpdaterStateDone, UpdaterErrorNone);
    FURI_LOG_I(TAG, "Done");
}

static void updater_process_block(Updater* updater, const UpdaterMessage* msg) {
    furi_check(updater);
    furi_check(msg);

    uint16_t crc_check = updater_crc16(msg->block.data, sizeof(msg->block.data), 0xFFFF);
    FURI_LOG_D(TAG, "block %u/%u", msg->block.block_idx, updater->update_params.blocks);

    if(crc_check != msg->block.block_crc) {
        FURI_LOG_E(TAG, "Block %u CRC mismatch: expected %04X, got %04X", msg->block.block_idx, msg->block.block_crc, crc_check);
        updater_state_change(updater, UpdaterStateError, UpdaterErrorBlockCrcError);
        return;
    }

    if((msg->block.block_idx >= updater->update_params.blocks) || (msg->block.block_idx != updater->current_block)) {
        FURI_LOG_E(TAG, "Block %u out of order: expected %u", msg->block.block_idx, updater->current_block);
        updater_state_change(updater, UpdaterStateError, UpdaterErrorBlockIndexError);
        return;
    }

    if(msg->block.block_idx < UPDATER_BLOCKS_PER_SECTOR) {
        memcpy(updater->first_sector_buf + (msg->block.block_idx * UPDATER_BLOCK_SIZE), msg->block.data, UPDATER_BLOCK_SIZE);
    } else if(!furi_hal_flash_write(
                  updater->inactive_partition_address + (msg->block.block_idx * UPDATER_BLOCK_SIZE),
                  msg->block.data,
                  UPDATER_BLOCK_SIZE)) {
        updater_state_change(updater, UpdaterStateError, UpdaterErrorFwWriteError);
        return;
    }

    updater->current_block++;

    if(updater->current_block == updater->update_params.blocks) {
        const uint8_t* flash_ptr = furi_hal_flash_get_read_ptr(updater->inactive_partition_address);
        uint16_t crc_final = updater_crc16(updater->first_sector_buf, FLASH_SECTOR_SIZE, 0xFFFF);
        crc_final = updater_crc16(
            flash_ptr + FLASH_SECTOR_SIZE,
            updater->update_params.blocks * UPDATER_BLOCK_SIZE - FLASH_SECTOR_SIZE,
            crc_final);
        if(crc_final != updater->update_params.crc) {
            FURI_LOG_E(TAG, "Image CRC mismatch: expected %04X, got %04X", updater->update_params.crc, crc_final);
            updater_state_change(updater, UpdaterStateError, UpdaterErrorFwCrcError);
            return;
        }

        updater_finalize_update(updater);

        furi_hal_flash_flush_cache();
    }
}

static uint32_t updater_isr_error_get_and_clear(Updater* updater) {
    FURI_CRITICAL_ENTER();
    uint32_t error = updater->isr_error;
    updater->isr_error = 0;
    FURI_CRITICAL_EXIT();
    return error;
}

static bool updater_promote_isr_error(Updater* updater) {
    if(!updater_isr_error_get_and_clear(updater)) return false;

    FURI_LOG_E(TAG, "I2C protocol error");
    updater->update_params.crc_set = false;
    updater->update_params.size_set = false;
    updater->update_params.blocks = 0;
    updater_state_change(updater, UpdaterStateError, UpdaterErrorProtocolError);
    return true;
}

static void updater_custom_event_callback(uint32_t events, void* context) {
    UNUSED(events);
    Updater* updater = context;
    furi_check(updater);

    updater_promote_isr_error(updater);
}

static void updater_handle_message(Updater* updater, const UpdaterMessage* msg);

// XIP stalls and IRQs are masked for each sector erase
static bool updater_erase_inactive_partition(Updater* updater) {
    FURI_LOG_I(TAG, "Erase start");
    for(size_t i = 0; i < updater->update_params.blocks; i += UPDATER_BLOCKS_PER_SECTOR) {
        // Serve pending messages so Abort interrupts the erase
        UpdaterMessage msg;
        while(furi_message_queue_get(updater->message_queue, &msg, 0) == FuriStatusOk) {
            if(msg.type == UpdaterMessageWriteBlockStart || msg.type == UpdaterMessageWriteBlockDone) {
                continue; // Reject block data until the erase finishes
            }
            updater_handle_message(updater, &msg);
            if(updater->state != UpdaterStateBusy) {
                FURI_LOG_I(TAG, "Erase interrupted");
                return false;
            }
        }
        furi_hal_flash_erase_sector(updater->inactive_partition_address + i * UPDATER_BLOCK_SIZE);
        furi_delay_ms(1); // Yield between sectors
    }
    FURI_LOG_I(TAG, "Erase done");
    return true;
}

static void updater_handle_message(Updater* updater, const UpdaterMessage* msg) {
    furi_check(updater);
    furi_check(msg);

    if(updater_promote_isr_error(updater)) return;

    if((msg->type == UpdaterMessageStartUpdate) && (updater->state == UpdaterStateIdle)) {
        if(!updater->update_supported) {
            FURI_LOG_E(TAG, "Update not supported: inactive partition unavailable");
            updater_state_change(updater, UpdaterStateError, UpdaterErrorNotStarted);
        } else if(updater->update_params.crc_set && updater->update_params.size_set) {
            if((updater->update_params.blocks < UPDATER_MIN_BLOCKS) ||
               (updater->update_params.blocks * UPDATER_BLOCK_SIZE > updater->inactive_partition_size)) {
                updater_state_change(updater, UpdaterStateError, UpdaterErrorSizeError);
            } else {
                updater_state_change(updater, UpdaterStateBusy, UpdaterErrorNone);
                updater->current_block = 0;
                if(updater_erase_inactive_partition(updater) && updater->state == UpdaterStateBusy) {
                    updater_state_change(updater, UpdaterStateRunning, UpdaterErrorNone);
                }
            }
        }
    } else if((msg->type == UpdaterMessageAbortUpdate) && (updater->state != UpdaterStateIdle)) {
        updater->update_params.crc_set = false;
        updater->update_params.size_set = false;
        updater->update_params.blocks = 0;
        updater_state_change(updater, UpdaterStateIdle, UpdaterErrorNone);
    } else if((msg->type == UpdaterMessageWriteBlockStart) && (updater->state == UpdaterStateRunning)) {
        updater_state_change(updater, UpdaterStateBusy, UpdaterErrorNone);
    } else if((msg->type == UpdaterMessageWriteBlockDone) && (updater->state == UpdaterStateBusy)) {
        // Running before the flash write lets the host pipeline the next block
        updater_state_change(updater, UpdaterStateRunning, UpdaterErrorNone);
        updater_process_block(updater, msg);
    } else if((msg->type == UpdaterMessageSetCrc) && (updater->state == UpdaterStateIdle)) {
        updater->update_params.crc = msg->fw_crc;
        updater->update_params.crc_set = true;
    } else if((msg->type == UpdaterMessageSetSize) && (updater->state == UpdaterStateIdle)) {
        updater->update_params.blocks = msg->fw_blocks;
        updater->update_params.size_set = true;
    }
}

static void updater_message_callback(FuriEventLoopObject* object, void* context) {
    furi_check(context);
    Updater* instance = context;
    furi_check(object == instance->message_queue);

    UpdaterMessage msg;
    furi_check(furi_message_queue_get(instance->message_queue, &msg, FuriWaitForever) == FuriStatusOk);

    updater_handle_message(instance, &msg);
}

int32_t updater_srv(void* p) {
    UNUSED(p);

    Updater* instance = malloc(sizeof(Updater));
    instance->event_loop = furi_event_loop_alloc();
    instance->message_queue = furi_message_queue_alloc(8, sizeof(UpdaterMessage));
    furi_event_loop_subscribe_message_queue(instance->event_loop, instance->message_queue, FuriEventLoopEventIn, updater_message_callback, instance);
    furi_event_loop_set_custom_event_callback(instance->event_loop, updater_custom_event_callback, instance);

    // Nothing below may crash: the negotiator blocks on RECORD_UPDATER
    instance->active_partition = furi_hal_flash_get_active_fw_partition();

    FlashPartitionId inactive_partition =
        instance->active_partition == FlashPartitionIdFwA ? FlashPartitionIdFwB : FlashPartitionIdFwA;
    instance->update_supported = furi_hal_flash_get_partition_info(
        inactive_partition, &instance->inactive_partition_address, &instance->inactive_partition_size);
    if(instance->update_supported) {
        instance->update_supported = instance->inactive_partition_address % FLASH_SECTOR_SIZE == 0;
    }
    if(!instance->update_supported) {
        FURI_LOG_E(TAG, "Inactive partition unavailable, updates disabled");
    }

    // Old images have no VERSION item; default to version 0
    size_t active_part_base = 0;
    bool active_partition_ok =
        furi_hal_flash_get_partition_info(instance->active_partition, &active_part_base, NULL);
    if(active_partition_ok &&
       furi_hal_flash_get_image_version(furi_hal_flash_get_read_ptr(active_part_base), instance->current_version.raw)) {
        FURI_LOG_I(
            TAG,
            "Current FW slot: %c, version: %04X, counter: %u",
            instance->active_partition == FlashPartitionIdFwA ? 'A' : 'B',
            instance->current_version.version,
            instance->current_version.counter);
    } else {
        FURI_LOG_W(TAG, "Active image has no version information, assuming version 0");
        instance->current_version.version = 0;
        instance->current_version.counter = 0;
    }

    // The negotiator reads current_version right after opening the record
    furi_record_create(RECORD_UPDATER, instance);

    furi_event_loop_run(instance->event_loop);

    return 0;
}

uint16_t updater_get_current_fw_version(Updater* updater) {
    furi_check(updater);
    return updater->current_version.version;
}

void updater_set_state_callback(Updater* updater, UpdaterStateCallback callback, void* context) {
    furi_check(updater);
    updater->state_callback = callback;
    updater->state_callback_context = context;
}
