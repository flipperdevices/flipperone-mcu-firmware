#include "furi_hal_flash.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include <core/common_defines.h>
#include <furi.h>
#include <hardware/flash.h>
// #include <furi_hal_cortex.h>
// #include <furi_hal_bits.h>

// #include "stm32u5xx.h"

// #define FURI_HAL_FLASH_TOTAL_PAGES (2 * FLASH_PAGE_NB)
// #define FURI_HAL_FLASH_BUSY_WAIT_TIMEOUT_US \
//     (1000000) // 1 second, for waiting for BSY flag to clear before an operation
// #define FURI_HAL_FLASH_PROGRAM_TIMEOUT_US \
//     (1000000) // 1 second, for the programming operation itself
// #define FURI_HAL_FLASH_ERASE_TIMEOUT_US (3000000) // 3 seconds, for the erase operation itself

static void furi_hal_flash_unlock() {
    // furi_check(FLASH->NSCR & FLASH_NSCR_LOCK);
    // FLASH->NSKEYR = 0x45670123U;
    // FLASH->NSKEYR = 0xCDEF89ABU;
    // furi_check(!(FLASH->NSCR & FLASH_NSCR_LOCK));
}

static void furi_hal_flash_ob_unlock() {
    // // must be called after unlocking flash
    // furi_check(!(FLASH->NSCR & FLASH_NSCR_LOCK));

    // // Unlock option bytes
    // // RM0456, 7.4.2 "Option-byte programming"
    // furi_check(FLASH->NSCR & FLASH_NSCR_OPTLOCK);
    // FLASH->OPTKEYR = 0x08192A3BU;
    // FLASH->OPTKEYR = 0x4C5D6E7FU;
    // furi_check(!(FLASH->NSCR & FLASH_NSCR_OPTLOCK));
}

static void furi_hal_flash_lock() {
    // FLASH->NSCR |= FLASH_NSCR_LOCK;
    // furi_check(FLASH->NSCR & FLASH_NSCR_LOCK);
}

size_t furi_hal_flash_get_page_size(void) {
    return FLASH_PAGE_SIZE;
}

size_t furi_hal_flash_get_base(void) {
    // Always return the base of the first bank (abstract both banks as a single region)
    return XIP_BASE;
}

const void* furi_hal_flash_get_free_end_address(void) {
   // return (void*)(FLASH_BASE_NS + (FLASH_PAGE_SIZE * FURI_HAL_FLASH_TOTAL_PAGES));
}

int16_t furi_hal_flash_get_page_number(size_t address) {
    // if(address < FLASH_BASE_NS) return -1;
    // size_t offset = address - FLASH_BASE_NS;
    // if(offset >= FLASH_PAGE_SIZE * FURI_HAL_FLASH_TOTAL_PAGES) return -1;
    // return offset / FLASH_PAGE_SIZE;
}

static bool furi_hal_flash_program_quad_word_remainder(
    uint32_t page_start_addr,
    uint16_t* bytes_programmed_total, // Pointer to update the total
    const uint8_t* data,
    uint16_t total_length) {
    // FuriHalCortexTimer timer;

    // while(*bytes_programmed_total < total_length) {
    //     uint32_t current_qword_addr = page_start_addr + *bytes_programmed_total;
    //     const uint8_t* data_source_ptr = data + *bytes_programmed_total;
    //     uint16_t bytes_left_to_program_in_page = total_length - *bytes_programmed_total;

    //     uint64_t dword_val1 = 0xFFFFFFFFFFFFFFFFULL;
    //     uint64_t dword_val2 = 0xFFFFFFFFFFFFFFFFULL;

    //     uint16_t count1 = (bytes_left_to_program_in_page < 8) ? bytes_left_to_program_in_page : 8;
    //     if(count1 > 0) {
    //         memcpy(&dword_val1, data_source_ptr, count1);
    //     }

    //     if(bytes_left_to_program_in_page > 8) {
    //         uint16_t count2 = ((bytes_left_to_program_in_page - 8) < 8) ?
    //                               (bytes_left_to_program_in_page - 8) :
    //                               8;
    //         memcpy(&dword_val2, data_source_ptr + 8, count2);
    //     }

    //     // 1. Wait for BSY = 0
    //     timer = furi_hal_cortex_timer_get(FURI_HAL_FLASH_BUSY_WAIT_TIMEOUT_US);
    //     while(FLASH->NSSR & FLASH_NSSR_BSY) {
    //         if(furi_hal_cortex_timer_is_expired(timer)) {
    //             furi_crash("BSY timeout: QW pre-write");
    //             return false; // Should not reach here due to furi_crash
    //         }
    //     }

    //     if(FLASH->NSSR & FLASH_NSSR_WDW) {
    //         FLASH->NSSR =
    //             (FLASH_NSSR_EOP | FLASH_NSSR_OPERR | FLASH_NSSR_PROGERR | FLASH_NSSR_WRPERR |
    //              FLASH_NSSR_PGAERR | FLASH_NSSR_SIZERR | FLASH_NSSR_PGSERR);
    //     }

    //     // 2. Wait until WDW is 0
    //     timer = furi_hal_cortex_timer_get(FURI_HAL_FLASH_BUSY_WAIT_TIMEOUT_US);
    //     while(FLASH->NSSR & FLASH_NSSR_WDW) {
    //         if(furi_hal_cortex_timer_is_expired(timer)) {
    //             furi_crash("WDW timeout: QW pre-write");
    //             return false;
    //         }
    //     }

    //     // 3. Clear relevant error flags
    //     FLASH->NSSR =
    //         (FLASH_NSSR_OPERR | FLASH_NSSR_PROGERR | FLASH_NSSR_WRPERR | FLASH_NSSR_PGAERR |
    //          FLASH_NSSR_SIZERR | FLASH_NSSR_PGSERR);

    //     // 4. Set PG bit
    //     FLASH->NSCR |= FLASH_NSCR_PG;

    //     // 5. Perform data write (one quad-word)
    //     *(volatile uint64_t*)(current_qword_addr) = dword_val1;
    //     *(volatile uint64_t*)(current_qword_addr + 8) = dword_val2;

    //     // 6. Wait until WDW is cleared
    //     timer = furi_hal_cortex_timer_get(FURI_HAL_FLASH_PROGRAM_TIMEOUT_US);
    //     while(FLASH->NSSR & FLASH_NSSR_WDW) {
    //         if(furi_hal_cortex_timer_is_expired(timer)) {
    //             furi_crash("WDW timeout: QW write");
    //             return false;
    //         }
    //     }

    //     // 7. Wait until BSY is cleared
    //     timer = furi_hal_cortex_timer_get(FURI_HAL_FLASH_PROGRAM_TIMEOUT_US);
    //     while(FLASH->NSSR & FLASH_NSSR_BSY) {
    //         if(furi_hal_cortex_timer_is_expired(timer)) {
    //             furi_crash("BSY clear timeout: QW write");
    //             return false;
    //         }
    //     }

    //     // 8. Clear EOP
    //     if(FLASH->NSSR & FLASH_NSSR_EOP) {
    //         FLASH->NSSR = FLASH_NSSR_EOP;
    //     }

    //     // Check for programming errors
    //     if((FLASH->NSSR & (FLASH_NSSR_OPERR | FLASH_NSSR_PROGERR | FLASH_NSSR_WRPERR |
    //                        FLASH_NSSR_PGAERR | FLASH_NSSR_SIZERR | FLASH_NSSR_PGSERR)) != 0) {
    //         furi_crash("error: QW write");
    //         return false;
    //     }

    //     // 9. Clear PG bit
    //     FLASH->NSCR &= ~FLASH_NSCR_PG;

    //     *bytes_programmed_total += 16;
    // }
    return true;
}

void furi_hal_flash_program_page(const uint8_t page, const uint8_t* data, uint16_t length) {
    // furi_check(page < FURI_HAL_FLASH_TOTAL_PAGES);
    // furi_check(data != NULL);
    // furi_check(length > 0 && length <= FLASH_PAGE_SIZE);

    // // Erase the page before programming.
    // furi_hal_flash_erase(page);

    // FURI_CRITICAL_ENTER();
    // furi_hal_flash_unlock();

    // uint32_t page_start_addr = FLASH_BASE_NS + (page * FLASH_PAGE_SIZE);
    // uint16_t bytes_programmed_total = 0;
    // FuriHalCortexTimer timer;

    // // --- Handle BURST programming part (128 bytes / 16 double-words per burst) ---
    // uint16_t num_bursts = length / 128;
    // for(uint16_t burst_idx = 0; burst_idx < num_bursts; ++burst_idx) {
    //     // 1. Wait for BSY = 0 (ensure no previous operation is ongoing)
    //     timer = furi_hal_cortex_timer_get(FURI_HAL_FLASH_BUSY_WAIT_TIMEOUT_US);
    //     while(FLASH->NSSR & FLASH_NSSR_BSY) {
    //         if(furi_hal_cortex_timer_is_expired(timer)) {
    //             furi_crash("BSY timeout: Burst pre-op");
    //         }
    //     }

    //     // 2. Ensure WDW (Write Data Window) is 0 before starting burst programming
    //     furi_check(!(FLASH->NSSR & FLASH_NSSR_WDW));

    //     // 3. Clear relevant error flags for *this specific upcoming* operation
    //     FLASH->NSSR =
    //         (FLASH_NSSR_OPERR | FLASH_NSSR_PROGERR | FLASH_NSSR_WRPERR | FLASH_NSSR_PGAERR |
    //          FLASH_NSSR_SIZERR | FLASH_NSSR_PGSERR);

    //     // 4. Set BWR (Burst Write) and PG (Programming) bits
    //     FLASH->NSCR |= (FLASH_NSCR_BWR | FLASH_NSCR_PG);

    //     // 5. Perform data write (16 double-words / 128 bytes)
    //     uint32_t current_burst_addr = page_start_addr + bytes_programmed_total;
    //     const uint8_t* current_data_ptr = data + bytes_programmed_total;

    //     for(int i = 0; i < 16; ++i) { // 16 double-words
    //         uint64_t dword_to_write;
    //         memcpy(&dword_to_write, current_data_ptr + (i * 8), 8);
    //         *(volatile uint64_t*)(current_burst_addr + (i * 8)) = dword_to_write;
    //     }

    //     // 6. Wait until BSY is set or WDW is cleared (operation started)
    //     timer = furi_hal_cortex_timer_get(FURI_HAL_FLASH_PROGRAM_TIMEOUT_US);
    //     while(!(FLASH->NSSR & FLASH_NSSR_BSY) && (FLASH->NSSR & FLASH_NSSR_WDW)) {
    //         if(furi_hal_cortex_timer_is_expired(timer)) {
    //             furi_crash("BSY/WDW timeout: Burst write");
    //         }
    //     }

    //     // 7. Wait until BSY is cleared (operation finished)
    //     timer = furi_hal_cortex_timer_get(FURI_HAL_FLASH_PROGRAM_TIMEOUT_US);
    //     while(FLASH->NSSR & FLASH_NSSR_BSY) {
    //         if(furi_hal_cortex_timer_is_expired(timer)) {
    //             furi_crash("BSY clear timeout: Burst write");
    //         }
    //     }

    //     // 8. If EOP flag is set in NSSR, clear it by writing 1 to it in NSSR
    //     if(FLASH->NSSR & FLASH_NSSR_EOP) {
    //         FLASH->NSSR = FLASH_NSSR_EOP;
    //     }

    //     // Check for programming errors
    //     furi_check(
    //         (FLASH->NSSR & (FLASH_NSSR_OPERR | FLASH_NSSR_PROGERR | FLASH_NSSR_WRPERR |
    //                         FLASH_NSSR_PGAERR | FLASH_NSSR_SIZERR | FLASH_NSSR_PGSERR)) == 0);

    //     // 9. Clear BWR and PG bits in FLASH_NSCR for the next operation (RM0456 Step 9 for burst)
    //     FLASH->NSCR &= ~(FLASH_NSCR_BWR | FLASH_NSCR_PG);

    //     bytes_programmed_total += 128;
    // }

    // // --- Handle REMAINDER programming part (non-burst, quad-words) ---
    // // Ensure BWR is clear for single programming operations. PG will be set per quad-word.
    // FLASH->NSCR &= ~FLASH_NSCR_BWR;

    // if(!furi_hal_flash_program_quad_word_remainder(
    //        page_start_addr, &bytes_programmed_total, data, length)) {
    //     // This path should ideally not be reached if furi_crash halts execution.
    //     furi_hal_flash_lock();
    //     FURI_CRITICAL_EXIT();
    //     return;
    // }

    // // Final cleanup: Ensure BWR and PG bits are clear.
    // FLASH->NSCR &= ~(FLASH_NSCR_BWR | FLASH_NSCR_PG);

    // furi_hal_flash_lock();
    // FURI_CRITICAL_EXIT();
}

bool furi_hal_flash_program_otp(const uint32_t base, const uint8_t* data, uint16_t length) {
    // furi_assert(base >= FLASH_OTP_BASE);
    // furi_assert((base + length) < (FLASH_OTP_BASE + FLASH_OTP_SIZE));
    // furi_assert((base & 0xF) == 0);
    // furi_assert(data);

    // // Check if OTP area is empty
    // for(uint32_t* ptr = (uint32_t*)base; ptr < (uint32_t*)(base + length); ptr++) {
    //     if(*ptr != 0xFFFFFFFF) {
    //         return false;
    //     }
    // }

    // FURI_CRITICAL_ENTER();
    // furi_hal_flash_unlock();

    // uint16_t bytes_programmed_total = 0;

    // // Ensure BWR is clear for single programming operations. PG will be set per quad-word.
    // FLASH->NSCR &= ~FLASH_NSCR_BWR;

    // if(!furi_hal_flash_program_quad_word_remainder(base, &bytes_programmed_total, data, length)) {
    //     // This path should ideally not be reached if furi_crash halts execution.
    //     furi_hal_flash_lock();
    //     FURI_CRITICAL_EXIT();
    //     return false;
    // }

    // // Final cleanup: Ensure BWR and PG bits are clear.
    // FLASH->NSCR &= ~(FLASH_NSCR_BWR | FLASH_NSCR_PG);

    // furi_hal_flash_lock();
    // FURI_CRITICAL_EXIT();

    return true;
}

// RM0456, 7.3.6 "Flash main memory erase sequences"
void furi_hal_flash_erase(const uint8_t page) {
    // furi_check(page < FURI_HAL_FLASH_TOTAL_PAGES);

    // FURI_CRITICAL_ENTER();

    // furi_hal_flash_unlock();

    // // Wait for any previous operation to finish
    // FuriHalCortexTimer timer = furi_hal_cortex_timer_get(FURI_HAL_FLASH_BUSY_WAIT_TIMEOUT_US);
    // while(FLASH->NSSR & FLASH_NSSR_BSY) {
    //     if(furi_hal_cortex_timer_is_expired(timer)) {
    //         furi_crash("BSY timeout: Erase pre-op");
    //     }
    // }

    // // Set page erase and page number
    // // For STM32U5, PER bit initiates page erase. PNB sets the page number.
    // // FLASH_NSCR_PNB is 7 bits wide, FLASH_NSCR_BKER selects the bank.
    // uint8_t bank = (page < FLASH_PAGE_NB) ? 0 : 1;
    // uint8_t page_in_bank = (page < FLASH_PAGE_NB) ? page : page - FLASH_PAGE_NB;

    // FLASH->NSCR &= ~(FLASH_NSCR_PNB | FLASH_NSCR_BKER); // Clear previous page and bank selection
    // if(bank == 0) {
    //     FLASH->NSCR &= ~FLASH_NSCR_BKER; // Select bank 1 (if BKER=0)
    // } else {
    //     FLASH->NSCR |= FLASH_NSCR_BKER; // Select bank 2 (if BKER=1)
    // }
    // FLASH->NSCR |= (page_in_bank << FLASH_NSCR_PNB_Pos) & FLASH_NSCR_PNB;
    // FLASH->NSCR |= FLASH_NSCR_PER; // Page erase operation
    // FLASH->NSCR |= FLASH_NSCR_STRT; // Start erase operation

    // // Wait for completion
    // timer = furi_hal_cortex_timer_get(FURI_HAL_FLASH_ERASE_TIMEOUT_US);
    // while(FLASH->NSSR & FLASH_NSSR_BSY) {
    //     if(furi_hal_cortex_timer_is_expired(timer)) {
    //         furi_crash("erase timeout");
    //     }
    // }

    // // Check for errors (only standard bits)
    // furi_check((FLASH->NSSR & (FLASH_NSSR_WRPERR | FLASH_NSSR_PGSERR | FLASH_NSSR_OPERR)) == 0);

    // // Clear PER bit
    // FLASH->NSCR &= ~FLASH_NSCR_PER;

    // furi_hal_flash_lock();
    // FURI_CRITICAL_EXIT();
}

void furi_hal_flash_init(void) {
    // // check that PA15_PUPEN is 0 and restore if necessary
    // if(furi_hal_bits_is_set(FLASH->OPTR, FLASH_OPTR_PA15_PUPEN)) {
    //     FuriHalCortexTimer timer = furi_hal_cortex_timer_get(FURI_HAL_FLASH_BUSY_WAIT_TIMEOUT_US);

    //     FURI_CRITICAL_ENTER();
    //     // Check that no flash memory operation is on going
    //     while(furi_hal_bits_is_set(FLASH->NSSR, FLASH_NSSR_BSY)) {
    //         if(furi_hal_cortex_timer_is_expired(timer)) {
    //             furi_crash("BSY timeout");
    //         }
    //     }

    //     // Clear OPTLOCK with the clearing sequence
    //     furi_hal_flash_unlock();
    //     furi_hal_flash_ob_unlock();

    //     // Clear PA15 PUPEN bit in option bytes
    //     furi_hal_bits_clear(&FLASH->OPTR, FLASH_OPTR_PA15_PUPEN);

    //     // Start option byte programming
    //     furi_hal_bits_set(&FLASH->NSCR, FLASH_NSCR_OPTSTRT);

    //     // Wait for the operation to complete
    //     while(furi_hal_bits_is_set(FLASH->NSSR, FLASH_NSSR_BSY)) {
    //         if(furi_hal_cortex_timer_is_expired(timer)) {
    //             furi_crash("BSY timeout: programming option bytes");
    //         }
    //     }

    //     // Check for option byte programming errors
    //     furi_check(furi_hal_bits_is_not_set(FLASH->NSSR, FLASH_NSSR_OPTWERR));
    //     furi_check(furi_hal_bits_is_not_set(FLASH->NSSR, FLASH_NSSR_PGSERR));

    //     // Load the new option bytes
    //     furi_hal_bits_set(&FLASH->NSCR, FLASH_NSCR_OBL_LAUNCH);

    //     // OPTLOCK is set when flash is locked
    //     furi_hal_flash_lock();

    //     FURI_CRITICAL_EXIT();

    //     furi_check(furi_hal_bits_is_not_set(FLASH->OPTR, FLASH_OPTR_PA15_PUPEN));
    // }
}
