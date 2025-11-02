#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/** Timer ISR */
typedef void (*FuriHalInterruptISR)(void* context);

typedef enum {
    // // SDMMC
     FuriHalInterruptIdSdMmc1,

    // // GPDMA
    // FuriHalInterruptIdGPDMA1Channel0,
    // FuriHalInterruptIdGPDMA1Channel1,
    // FuriHalInterruptIdGPDMA1Channel2,
    // FuriHalInterruptIdGPDMA1Channel3,
    // FuriHalInterruptIdGPDMA1Channel4,
    // FuriHalInterruptIdGPDMA1Channel5,
    // FuriHalInterruptIdGPDMA1Channel6,
    // FuriHalInterruptIdGPDMA1Channel7,
    // FuriHalInterruptIdGPDMA1Channel8,
    // FuriHalInterruptIdGPDMA1Channel9,
    // FuriHalInterruptIdGPDMA1Channel10,
    // FuriHalInterruptIdGPDMA1Channel11,
    // FuriHalInterruptIdGPDMA1Channel12,
    // FuriHalInterruptIdGPDMA1Channel13,
    // FuriHalInterruptIdGPDMA1Channel14,
    // FuriHalInterruptIdGPDMA1Channel15,

    // // LPDMA
    // FuriHalInterruptIdLPDMA1Channel0,
    // FuriHalInterruptIdLPDMA1Channel1,
    // FuriHalInterruptIdLPDMA1Channel2,
    // FuriHalInterruptIdLPDMA1Channel3,

    // // GPU
    // FuriHalInterruptIdDMA2D,
    // FuriHalInterruptIdGPU2D,
    // FuriHalInterruptIdGPU2DError,

    // // USART
    // FuriHalInterruptIdUsart1,
    // FuriHalInterruptIdUsart2,
    // FuriHalInterruptIdUsart3,
    // FuriHalInterruptIdUsart6,

    // // UART
    // FuriHalInterruptIdUart4,
    // FuriHalInterruptIdUart5,

    // // RCC
    // FuriHalInterruptIdRcc,

    // // LPUART
    // FuriHalInterruptIdLPUART1,

    // // LPTIM
    // FuriHalInterruptIdLPTIM1,
    // FuriHalInterruptIdLPTIM2,
    // FuriHalInterruptIdLPTIM3,
    // FuriHalInterruptIdLPTIM4,

    // // USB + PD
    // FuriHalInterruptIdUCPD1,
    // FuriHalInterruptIdUSBHS,

    // Service value
    FuriHalInterruptIdMax,
} FuriHalInterruptId;

typedef enum {
    FuriHalInterruptPriorityLowest =
        -3, /**< Lowest priority level, you can use ISR-safe OS primitives */
    FuriHalInterruptPriorityLower =
        -2, /**< Lower priority level, you can use ISR-safe OS primitives */
    FuriHalInterruptPriorityLow =
        -1, /**< Low priority level, you can use ISR-safe OS primitives */
    FuriHalInterruptPriorityNormal =
        0, /**< Normal(default) priority level, you can use ISR-safe OS primitives */
    FuriHalInterruptPriorityHigh =
        1, /**< High priority level, you can use ISR-safe OS primitives */
    FuriHalInterruptPriorityHigher =
        2, /**< Higher priority level, you can use ISR-safe OS primitives */
    FuriHalInterruptPriorityHighest =
        3, /**< Highest priority level, you can use ISR-safe OS primitives */

    /* Special group, read docs first(ALL OF THEM: especially FreeRTOS configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY) */
    FuriHalInterruptPriorityKamiSama =
        6, /**< Forget about thread safety, you are god now. No one can prevent you from messing with OS critical section. You are not allowed to use any OS primitives, but who can stop you? Use this priority only for direct hardware interaction with LL HAL. */
} FuriHalInterruptPriority;

/** Initialize interrupt subsystem */
void furi_hal_interrupt_init(void);

/** Set ISR and enable interrupt with default priority
 *
 * @warning    Interrupt flags are not cleared automatically. You may want to
 *             ensure that your peripheral status flags are cleared.
 *
 * @param      index    - interrupt ID
 * @param      isr      - your interrupt service routine or use NULL to clear
 * @param      context  - isr context
 * @warning    All interrupts set by this function use priority FuriHalInterruptPriorityNormal.
 *             When called before FreeRTOS scheduler is started, this function will
 *             set the interrupt priority to FuriHalInterruptPriorityKamiSama.
 */
void furi_hal_interrupt_set_isr(FuriHalInterruptId index, FuriHalInterruptISR isr, void* context);

/** Set ISR and enable interrupt with custom priority
 *
 * @warning    Interrupt flags are not cleared automatically. You may want to
 *             ensure that your peripheral status flags are cleared.
 *
 * @param      index     - interrupt ID
 * @param      priority  - One of FuriHalInterruptPriority
 * @param      isr       - your interrupt service routine or use NULL to clear
 * @param      context   - isr context
 * @note       Before FreeRTOS scheduler is started, only ISRs with priority 
 *             `FuriHalInterruptPriorityKamiSama` will trigger.
 */
void furi_hal_interrupt_set_isr_ex(
    FuriHalInterruptId index,
    FuriHalInterruptPriority priority,
    FuriHalInterruptISR isr,
    void* context);

/** Get interrupt name by exception number.
 * Exception number can be obtained from IPSR register.
 * 
 * @param exception_number 
 * @return const char* or NULL if interrupt name is not found
 */
const char* furi_hal_interrupt_get_name(uint8_t exception_number);

/** Get total time(in CPU clocks) spent in ISR
 *
 * @return     total time in CPU clocks
 */
uint32_t furi_hal_interrupt_get_time_in_isr_total(void);

#ifdef __cplusplus
}
#endif
