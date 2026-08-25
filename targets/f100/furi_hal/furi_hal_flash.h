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

/** Check if firmware partition is valid (contains IMAGE_DEF block)
 *
 * @param[in]  partition_id  partition id
 * @return     true if partition is valid, false otherwise
 */
bool furi_hal_flash_fw_partition_is_valid(FlashPartitionId partition_id);

/** Invalidate firmware partition (clear IMAGE_DEF block header)
 *
 * @param[in]  partition_id  partition id
 * @return     true if partition was invalidated, false otherwise
 */
bool furi_hal_flash_fw_partition_invalidate(FlashPartitionId partition_id);

/** Get version from IMAGE_DEF of firmware image
 *
 * @param[in]  start    pointer to the first firmware sector
 * @param[out] version  pointer to array of two uint16_t to store version and counter
 * @return     true if version was found, false if IMAGE_DEF was not found or invalid
 */
bool furi_hal_flash_get_image_version(const uint8_t* start, uint16_t* version);

/** Change version in IMAGE_DEF of firmware image
 *
 * @param[in]  start    pointer to the first firmware sector
 * @param[in]  version  pointer to array of two uint16_t containing version and counter
 * @return     true if version was found, false if IMAGE_DEF was not found or invalid
 */
bool furi_hal_flash_set_image_version(uint8_t* start, const uint16_t* version);

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

/** Erase flash sector (4096 bytes)
 *
 * @param[in]  address  address of the sector to erase
 */
void furi_hal_flash_erase_sector(size_t address);

/** Write data to flash
 *
 * @param[in]  address  address to write to
 * @param[in]  data     pointer to data to write
 * @param[in]  size     size of data to write, must be multiple of flash page size (256 bytes)
 */
void furi_hal_flash_write(size_t address, const void* data, size_t size);

/** Read data from flash
 *
 * @param[in]  address  address to read from
 * @param[out] data     pointer to buffer to read data into
 * @param[in]  size     size of data to read
 */
void furi_hal_flash_read(size_t address, void* data, size_t size);

/** Get pointer to flash data for direct non-cached read
 *
 * @param[in]  address  address to get pointer to
 * @return     pointer to flash data at the given address
 */
const uint8_t* furi_hal_flash_get_read_ptr(size_t address);

/** Flush flash cache
 *
 * This function should be called after writing to flash to ensure that the changes are visible to the CPU.
 */
void furi_hal_flash_flush_cache(void);

#ifdef __cplusplus
}
#endif
