#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FlashPartitionIdFwA = 0,
    FlashPartitionIdFwB = 1,
    FlashPartitionIdStorage = 2,
} FlashPartitionId;

/** Get flash partition info
 *
 * @param[in]  partition_id  partition id
 * @param[out] base          pointer to base address of the partition
 * @param[out] size          pointer to size of the partition
 * @return     true if partition was found, false otherwise
 */
bool furi_hal_flash_get_partition_info(FlashPartitionId partition_id, size_t* base, size_t* size);

/** Get partition id from which we are running now
 *
 * @return     partition id of the active firmware
 */
FlashPartitionId furi_hal_flash_get_active_fw_partition(void);

/** Get flash base address
 *
 * @return     pointer to flash base
 */
size_t furi_hal_flash_get_base(void);

/** Get flash page size
 *
 * @return     size in bytes
 */
size_t furi_hal_flash_get_page_size(void);

/** Get free flash end address
 *
 * @return     pointer to free region end
 */
const void* furi_hal_flash_get_free_end_address(void);

#ifdef __cplusplus
}
#endif
