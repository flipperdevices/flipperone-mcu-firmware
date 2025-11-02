#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/** Timer ISR */
typedef void (*FuriHalInterruptISR)(void* context);

typedef enum {
    // Timer IRQs
    FuriHalInterruptIdTimer0Irq0,
    FuriHalInterruptIdTimer0Irq1,
    FuriHalInterruptIdTimer0Irq2,
    FuriHalInterruptIdTimer0Irq3,
    FuriHalInterruptIdTimer1Irq0,
    FuriHalInterruptIdTimer1Irq1,
    FuriHalInterruptIdTimer1Irq2,
    FuriHalInterruptIdTimer1Irq3,
    // PWM IRQs
    FuriHalInterruptIdPwmWrap0,
    FuriHalInterruptIdPwmWrap1,
    // DMA IRQs
    FuriHalInterruptIdDmaChannel0,
    FuriHalInterruptIdDmaChannel1,
    FuriHalInterruptIdDmaChannel2,
    FuriHalInterruptIdDmaChannel3,
    // USBCTRL
    FuriHalInterruptIdUsbCtrl,
    // PIO0
    FuriHalInterruptIdPio0Irq0,
    FuriHalInterruptIdPio0Irq1,
    // PIO1
    FuriHalInterruptIdPio1Irq0,
    FuriHalInterruptIdPio1Irq1,
    // PIO2
    FuriHalInterruptIdPio2Irq0,
    FuriHalInterruptIdPio2Irq1,
    // IO_BANK0
    FuriHalInterruptIdIoBank0,
    FuriHalInterruptIdIoBank0Ns,
    // IO_QSPI
    FuriHalInterruptIdIoQspi,
    FuriHalInterruptIdIoQspiNs,
    // SIO
    FuriHalInterruptIdSioFifo,
    FuriHalInterruptIdSioBell,
    FuriHalInterruptIdSioFifoNs,
    FuriHalInterruptIdSioBellNs,
    FuriHalInterruptIdSioMtimecmp,
    // CLOCKS
    FuriHalInterruptIdClocks,
    // SPI
    FuriHalInterruptIdSpi0,
    FuriHalInterruptIdSpi1,
    // UART
    FuriHalInterruptIdUart0,
    FuriHalInterruptIdUart1,
    // ADC
    FuriHalInterruptIdAdcFifo,
    // I2C
    FuriHalInterruptIdI2c0,
    FuriHalInterruptIdI2c1,
    // OTP
    FuriHalInterruptIdOtp,
    // TRNG
    FuriHalInterruptIdTrng,
    // PROC CTI
    FuriHalInterruptIdProc0Cti,
    FuriHalInterruptIdProc1Cti,
    // PLL
    FuriHalInterruptIdPllSys,
    FuriHalInterruptIdPllUsb,
    // POWMAN
    FuriHalInterruptIdPowmanPow,
    FuriHalInterruptIdPowmanTimer,
    // SPARE IRQs
    FuriHalInterruptIdSpareIrq0,
    FuriHalInterruptIdSpareIrq1,
    FuriHalInterruptIdSpareIrq2,
    FuriHalInterruptIdSpareIrq3,
    FuriHalInterruptIdSpareIrq4,
    FuriHalInterruptIdSpareIrq5,

    // Service value
    FuriHalInterruptIdMax,
} FuriHalInterruptId;

typedef enum {
    FuriHalInterruptPriorityLowest = -3, /**< Lowest priority level, you can use ISR-safe OS primitives */
    FuriHalInterruptPriorityLower = -2, /**< Lower priority level, you can use ISR-safe OS primitives */
    FuriHalInterruptPriorityLow = -1, /**< Low priority level, you can use ISR-safe OS primitives */
    FuriHalInterruptPriorityNormal = 0, /**< Normal(default) priority level, you can use ISR-safe OS primitives */
    FuriHalInterruptPriorityHigh = 1, /**< High priority level, you can use ISR-safe OS primitives */
    FuriHalInterruptPriorityHigher = 2, /**< Higher priority level, you can use ISR-safe OS primitives */
    FuriHalInterruptPriorityHighest = 3, /**< Highest priority level, you can use ISR-safe OS primitives */

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
void furi_hal_interrupt_set_isr_ex(FuriHalInterruptId index, FuriHalInterruptPriority priority, FuriHalInterruptISR isr, void* context);

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
