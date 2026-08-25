#include "updater.h"
#include <furi_hal_flash.h>
#include <bit_lib/bit_lib.h>
#include <hardware/flash.h>

#define TAG "Updater"

typedef union {
    struct {
        uint16_t version;
        uint16_t counter;
    };
    uint16_t raw[2];
} ImageVersion;

struct Updater {
    FuriEventLoop* event_loop;
    FuriMessageQueue* message_queue;
    UpdaterState state;

    uint16_t fw_crc;
    uint16_t fw_blocks;
    bool crc_set;
    bool size_set;

    uint16_t current_block;

    FlashPartitionId active_partition;
    ImageVersion current_version;

    size_t write_partition_address;
    size_t write_partition_size;
    uint8_t first_sector_buf[FLASH_SECTOR_SIZE];

    UpdaterStateCallback state_callback;
    void* state_callback_context;

    union {
        struct {
            uint8_t data[256];
            uint16_t idx;
            uint16_t crc16;
        };
        uint8_t raw[256 + 2 + 2];
    } i2c_data;
    size_t i2c_data_offset;
};
static_assert(sizeof(((struct Updater*)0)->i2c_data.raw) == (sizeof(((struct Updater*)0)->i2c_data.data) + 2 + 2));

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
            uint8_t data[256];
        } block;
    };
} UpdaterMessage;

// I2C write callbacks, called from interrupt context
void updater_on_i2c_crc_reg_write(void* context, uint16_t address, uint16_t value) {
    Updater* updater = context;
    furi_check(updater);

    UpdaterMessage msg = {
        .type = UpdaterMessageSetCrc,
        .fw_crc = value,
    };
    furi_check(furi_message_queue_put(updater->message_queue, &msg, 0) == FuriStatusOk);
}

void updater_on_i2c_blocks_reg_write(void* context, uint16_t address, uint16_t value) {
    Updater* updater = context;
    furi_check(updater);

    UpdaterMessage msg = {
        .type = UpdaterMessageSetSize,
        .fw_blocks = value,
    };
    furi_check(furi_message_queue_put(updater->message_queue, &msg, 0) == FuriStatusOk);
}

void updater_on_i2c_command_reg_write(void* context, uint16_t address, uint16_t value) {
    Updater* updater = context;
    furi_check(updater);

    UpdaterMessage msg;
    msg.type = (value == UpdaterCommandStart) ? UpdaterMessageStartUpdate : UpdaterMessageAbortUpdate;
    furi_check(furi_message_queue_put(updater->message_queue, &msg, 0) == FuriStatusOk);
}

bool updater_on_i2c_data_write(void* context, uint16_t offset, uint8_t value) {
    Updater* updater = context;
    furi_check(updater);

    if(offset >= sizeof(updater->i2c_data.raw)) return false;

    if(offset == 0) {
        memset(updater->i2c_data.raw, 0, sizeof(updater->i2c_data.raw));
        updater->i2c_data_offset = 0;
        UpdaterMessage msg = {.type = UpdaterMessageWriteBlockStart};
        furi_check(furi_message_queue_put(updater->message_queue, &msg, 0) == FuriStatusOk);
    }

    if(offset != updater->i2c_data_offset) return false; // Out-of-order write

    updater->i2c_data.raw[updater->i2c_data_offset++] = value;

    if(updater->i2c_data_offset == sizeof(updater->i2c_data.raw)) {
        UpdaterMessage msg = {
            .type = UpdaterMessageWriteBlockDone,
            .block.block_idx = updater->i2c_data.idx,
            .block.block_crc = updater->i2c_data.crc16,
        };
        memcpy(msg.block.data, updater->i2c_data.data, sizeof(msg.block.data));
        furi_check(furi_message_queue_put(updater->message_queue, &msg, 0) == FuriStatusOk);
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

void updater_write_flash_block(Updater* updater, uint16_t block_idx, uint8_t* data) {
    furi_check(updater);
    furi_check(data);

    size_t write_address = updater->write_partition_address + (block_idx * 256);
    if(write_address % FLASH_SECTOR_SIZE == 0) {
        furi_hal_flash_erase_sector(write_address);
    }
    if(block_idx < FLASH_SECTOR_SIZE / 256) {
        memcpy(updater->first_sector_buf + (block_idx * 256), data, 256);
    } else {
        furi_hal_flash_write(write_address, data, 256);
    }
}

static void updater_write_first_sector(Updater* updater) {
    ImageVersion new_version;
    bool success = furi_hal_flash_get_image_version(updater->first_sector_buf, new_version.raw);
    if(!success) {
        FURI_LOG_E(TAG, "Update image missing version information");
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

    furi_hal_flash_write(updater->write_partition_address, updater->first_sector_buf, FLASH_SECTOR_SIZE);

    if(do_downgrade) {
        furi_hal_flash_fw_partition_invalidate(updater->active_partition);
    }

    updater_state_change(updater, UpdaterStateDone, UpdaterErrorNone);
}

static void updater_process_block(Updater* updater, UpdaterMessage* msg) {
    furi_check(updater);
    furi_check(msg);

    uint16_t crc_check = updater_crc16(msg->block.data, sizeof(msg->block.data), 0xFFFF);
    FURI_LOG_I(TAG, "block %u/%u", msg->block.block_idx, updater->fw_blocks);

    if(crc_check != msg->block.block_crc) {
        FURI_LOG_E(TAG, "Block %u CRC mismatch: expected %04X, got %04X", msg->block.block_idx, msg->block.block_crc, crc_check);
        updater_state_change(updater, UpdaterStateError, UpdaterErrorBlockCrcError);
        return;
    }

    if((msg->block.block_idx >= updater->fw_blocks) || (msg->block.block_idx != updater->current_block)) {
        FURI_LOG_E(TAG, "Block %u out of order: expected %u", msg->block.block_idx, updater->current_block);
        updater_state_change(updater, UpdaterStateError, UpdaterErrorBlockIndexError);
        return;
    }

    updater_write_flash_block(updater, msg->block.block_idx, msg->block.data);

    updater->current_block++;

    if(updater->current_block == updater->fw_blocks) {
        const uint8_t* flash_ptr = furi_hal_flash_get_read_ptr(updater->write_partition_address);
        uint16_t crc_final = updater_crc16(updater->first_sector_buf, FLASH_SECTOR_SIZE, 0xFFFF);
        crc_final = updater_crc16((uint8_t*)flash_ptr + FLASH_SECTOR_SIZE, updater->fw_blocks * 256 - FLASH_SECTOR_SIZE, crc_final);
        if(crc_final != updater->fw_crc) {
            FURI_LOG_E(TAG, "Image CRC mismatch: expected %04X, got %04X", updater->fw_crc, crc_final);
            updater_state_change(updater, UpdaterStateError, UpdaterErrorFwCrcError);
            return;
        }

        updater_write_first_sector(updater);

        furi_hal_flash_flush_cache();
    }
}

static void updater_message_callback(FuriEventLoopObject* object, void* context) {
    furi_check(context);
    Updater* instance = context;
    furi_check(object == instance->message_queue);

    UpdaterMessage msg;
    furi_check(furi_message_queue_get(instance->message_queue, &msg, FuriWaitForever) == FuriStatusOk);

    if((msg.type == UpdaterMessageStartUpdate) && (instance->state == UpdaterStateIdle)) {
        if(instance->crc_set && instance->size_set) {
            if((instance->fw_blocks < (FLASH_SECTOR_SIZE * 4 / 256)) || (instance->fw_blocks * 256 > instance->write_partition_size)) {
                updater_state_change(instance, UpdaterStateError, UpdaterErrorSizeError);
            } else {
                instance->current_block = 0;
                updater_state_change(instance, UpdaterStateRunning, UpdaterErrorNone);
            }
        }
    } else if((msg.type == UpdaterMessageAbortUpdate) && (instance->state != UpdaterStateIdle)) {
        instance->crc_set = false;
        instance->size_set = false;
        instance->fw_blocks = 0;
        updater_state_change(instance, UpdaterStateIdle, UpdaterErrorNone);
    } else if((msg.type == UpdaterMessageWriteBlockStart) && (instance->state == UpdaterStateRunning)) {
        updater_state_change(instance, UpdaterStateBusy, UpdaterErrorNone);
    } else if((msg.type == UpdaterMessageWriteBlockDone) && (instance->state == UpdaterStateBusy)) {
        updater_state_change(instance, UpdaterStateRunning, UpdaterErrorNone);
        updater_process_block(instance, &msg);
    } else if(msg.type == UpdaterMessageSetCrc) {
        instance->fw_crc = msg.fw_crc;
        instance->crc_set = true;
    } else if(msg.type == UpdaterMessageSetSize) {
        instance->fw_blocks = msg.fw_blocks;
        instance->size_set = true;
    }
}

int32_t updater_srv(void* p) {
    UNUSED(p);

    Updater* instance = malloc(sizeof(Updater));
    instance->event_loop = furi_event_loop_alloc();
    instance->message_queue = furi_message_queue_alloc(2, sizeof(UpdaterMessage));
    furi_event_loop_subscribe_message_queue(instance->event_loop, instance->message_queue, FuriEventLoopEventIn, updater_message_callback, instance);

    updater_state_change(instance, UpdaterStateIdle, UpdaterErrorNone);
    instance->active_partition = furi_hal_flash_get_active_fw_partition();
    furi_hal_flash_get_partition_info(
        instance->active_partition == FlashPartitionIdFwA ? FlashPartitionIdFwB : FlashPartitionIdFwA,
        &instance->write_partition_address,
        &instance->write_partition_size);
    furi_check(instance->write_partition_address % FLASH_SECTOR_SIZE == 0);

    size_t active_part_base = 0;
    bool ret = furi_hal_flash_get_partition_info(instance->active_partition, &active_part_base, NULL);
    furi_check(ret);

    const uint8_t* part_ptr = furi_hal_flash_get_read_ptr(active_part_base);

    furi_check(furi_hal_flash_get_image_version(part_ptr, instance->current_version.raw));
    FURI_LOG_I(TAG, "Current FW version: %04X, counter: %u", instance->current_version.version, instance->current_version.counter);

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
