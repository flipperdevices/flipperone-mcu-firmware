#include "updater.h"
#include <furi_hal_flash.h>

#define TAG "UpdaterSrv"

// u32 offset
// u32 crc
// [256] data

struct Updater {
    FuriEventLoop* event_loop;
    FuriMessageQueue* message_queue;
    enum {
        UpdaterStateIdle,
        UpdaterStateError,
        UpdaterStateRunning,
        UpdaterStateDone,
    } state;
    uint32_t image_crc;
    uint32_t image_blocks;
    uint32_t current_block;
    FlashPartitionId active_partition;
};

typedef struct {
    enum {
        UpdaterMessageHead,
        UpdaterMessageBlock,

    } type;
    union {
        struct {
            uint32_t block_count;
            uint32_t image_crc;
        } head;

        struct {
            uint32_t block_idx;
            uint32_t block_crc;
            uint8_t data[256];
        } block;
    };
} UpdaterMessage;

static void updater_message_callback(FuriEventLoopObject* object, void* context) {
    furi_check(context);
    Updater* instance = context;
    furi_check(object == instance->message_queue);
}

int32_t updater_srv(void* p) {
    UNUSED(p);
    Updater* instance = malloc(sizeof(Updater));
    instance->event_loop = furi_event_loop_alloc();
    instance->message_queue = furi_message_queue_alloc(1, sizeof(uint32_t));
    furi_event_loop_subscribe_message_queue(instance->event_loop, instance->message_queue, FuriEventLoopEventIn, updater_message_callback, instance);

    instance->state = UpdaterStateIdle;
    instance->active_partition = furi_hal_flash_get_active_fw_partition();

    furi_event_loop_run(instance->event_loop);

    return 0;
}

// #include "pico/bootrom.h"
// #include "pico/bootrom_constants.h"
// #include "hardware/flash.h"
// #include <stdio.h>

// static uint8_t workarea[4096] __attribute__((aligned(4)));

// void updater_write_test(void) {
//     resident_partition_t target_part;

//     int rc = rom_get_uf2_target_partition(workarea, sizeof(workarea), FAMILY_ID, &target_part);
//     if(rc < 0) {
//         return;
//     }

//     uint16_t first_sector = (target_part.permissions_and_location & PICOBIN_PARTITION_LOCATION_FIRST_SECTOR_BITS) >>
//                             PICOBIN_PARTITION_LOCATION_FIRST_SECTOR_LSB;
//     uint16_t last_sector = (target_part.permissions_and_location & PICOBIN_PARTITION_LOCATION_LAST_SECTOR_BITS) >> PICOBIN_PARTITION_LOCATION_LAST_SECTOR_LSB;

//     uint32_t code_start_addr = first_sector * FLASH_SECTOR_SIZE;
//     uint32_t code_end_addr = (last_sector + 1) * FLASH_SECTOR_SIZE;

//     cflash_flags_t flags;
//     flags.flags = (CFLASH_OP_VALUE_ERASE << CFLASH_OP_LSB) | (CFLASH_SECLEVEL_VALUE_SECURE << CFLASH_SECLEVEL_LSB) |
//                   (CFLASH_ASPACE_VALUE_STORAGE << CFLASH_ASPACE_LSB);

//     for(uint32_t addr = code_start_addr; addr < code_end_addr; addr += FLASH_SECTOR_SIZE) {
//         int ret = rom_flash_op(flags, addr, FLASH_SECTOR_SIZE, NULL);
//         if(ret < 0) return;
//     }

//     flags.flags = (CFLASH_OP_VALUE_PROGRAM << CFLASH_OP_LSB) | (CFLASH_SECLEVEL_VALUE_SECURE << CFLASH_SECLEVEL_LSB) |
//                   (CFLASH_ASPACE_VALUE_STORAGE << CFLASH_ASPACE_LSB);

//     for(uint32_t offset = 0; offset < firmware_size; offset += FLASH_PAGE_SIZE) {
//         int ret = rom_flash_op(flags, code_start_addr + offset, FLASH_PAGE_SIZE, (uint8_t*)(new_firmware + offset));
//         if(ret < 0) return;
//     }

//     rom_flash_flush_cache();
//     rom_reboot(REBOOT2_FLAG_REBOOT_TYPE_FLASH_UPDATE, 1000, code_start_addr, 0);
// }
