#include <furi_hal.h>
#include <FreeRTOS.h>
#include <hardware/structs/resets.h>
// #include <stm32u5xx_ll_cortex.h>
// #include <stm32u5xx_ll_system.h>
#include <hardware/irq.h>

#define TAG "FuriHalInterrupt"

#define FURI_HAL_INTERRUPT_DEFAULT_PRIORITY (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY + 5)

typedef struct {
    FuriHalInterruptISR isr;
    void* context;
} FuriHalInterruptISRPair;

FuriHalInterruptISRPair furi_hal_interrupt_isr[FuriHalInterruptIdMax] = {0};

const IRQn_Type furi_hal_interrupt_irqn[FuriHalInterruptIdMax] = {
    [FuriHalInterruptIdTimer0Irq0] = TIMER0_IRQ_0,
    [FuriHalInterruptIdTimer0Irq1] = TIMER0_IRQ_1,
    [FuriHalInterruptIdTimer0Irq2] = TIMER0_IRQ_2,
    [FuriHalInterruptIdTimer0Irq3] = TIMER0_IRQ_3,
    [FuriHalInterruptIdTimer1Irq0] = TIMER1_IRQ_0,
    [FuriHalInterruptIdTimer1Irq1] = TIMER1_IRQ_1,
    [FuriHalInterruptIdTimer1Irq2] = TIMER1_IRQ_2,
    [FuriHalInterruptIdTimer1Irq3] = TIMER1_IRQ_3,
    [FuriHalInterruptIdPwmWrap0] = PWM_IRQ_WRAP_0,
    [FuriHalInterruptIdPwmWrap1] = PWM_IRQ_WRAP_1,
    [FuriHalInterruptIdDmaChannel0] = DMA_IRQ_0,
    [FuriHalInterruptIdDmaChannel1] = DMA_IRQ_1,
    [FuriHalInterruptIdDmaChannel2] = DMA_IRQ_2,
    [FuriHalInterruptIdDmaChannel3] = DMA_IRQ_3,
    [FuriHalInterruptIdUsbCtrl] = USBCTRL_IRQ,
    [FuriHalInterruptIdPio0Irq0] = PIO0_IRQ_0,
    [FuriHalInterruptIdPio0Irq1] = PIO0_IRQ_1,
    [FuriHalInterruptIdPio1Irq0] = PIO1_IRQ_0,
    [FuriHalInterruptIdPio1Irq1] = PIO1_IRQ_1,
    [FuriHalInterruptIdPio2Irq0] = PIO2_IRQ_0,
    [FuriHalInterruptIdPio2Irq1] = PIO2_IRQ_1,
    [FuriHalInterruptIdIoBank0] = IO_IRQ_BANK0,
    [FuriHalInterruptIdIoBank0Ns] = IO_IRQ_BANK0_NS,
    [FuriHalInterruptIdIoQspi] = IO_IRQ_QSPI,
    [FuriHalInterruptIdIoQspiNs] = IO_IRQ_QSPI_NS,
    [FuriHalInterruptIdSioFifo] = SIO_IRQ_FIFO,
    [FuriHalInterruptIdSioBell] = SIO_IRQ_BELL,
    [FuriHalInterruptIdSioFifoNs] = SIO_IRQ_FIFO_NS,
    [FuriHalInterruptIdSioBellNs] = SIO_IRQ_BELL_NS,
    [FuriHalInterruptIdSioMtimecmp] = SIO_IRQ_MTIMECMP,
    [FuriHalInterruptIdClocks] = CLOCKS_IRQ,
    [FuriHalInterruptIdSpi0] = SPI0_IRQ,
    [FuriHalInterruptIdSpi1] = SPI1_IRQ,
    [FuriHalInterruptIdUart0] = UART0_IRQ,
    [FuriHalInterruptIdUart1] = UART1_IRQ,
    [FuriHalInterruptIdAdcFifo] = ADC_IRQ_FIFO,
    [FuriHalInterruptIdI2c0] = I2C0_IRQ,
    [FuriHalInterruptIdI2c1] = I2C1_IRQ,
    [FuriHalInterruptIdOtp] = OTP_IRQ,
    [FuriHalInterruptIdTrng] = TRNG_IRQ,
    [FuriHalInterruptIdProc0Cti] = PROC0_IRQ_CTI,
    [FuriHalInterruptIdProc1Cti] = PROC1_IRQ_CTI,
    [FuriHalInterruptIdPllSys] = PLL_SYS_IRQ,
    [FuriHalInterruptIdPllUsb] = PLL_USB_IRQ,
    [FuriHalInterruptIdPowmanPow] = POWMAN_IRQ_POW,
    [FuriHalInterruptIdPowmanTimer] = POWMAN_IRQ_TIMER,
    [FuriHalInterruptIdSpareIrq0] = SPARE_IRQ_0,
    [FuriHalInterruptIdSpareIrq1] = SPARE_IRQ_1,
    [FuriHalInterruptIdSpareIrq2] = SPARE_IRQ_2,
    [FuriHalInterruptIdSpareIrq3] = SPARE_IRQ_3,
    [FuriHalInterruptIdSpareIrq4] = SPARE_IRQ_4,
    [FuriHalInterruptIdSpareIrq5] = SPARE_IRQ_5,
};

__attribute__((always_inline)) static inline void furi_hal_interrupt_call(FuriHalInterruptId index) {
    furi_check(furi_hal_interrupt_isr[index].isr);
    furi_hal_interrupt_isr[index].isr(furi_hal_interrupt_isr[index].context);
}

__attribute__((always_inline)) static inline void furi_hal_interrupt_enable(FuriHalInterruptId index, uint16_t priority) {
    NVIC_SetPriority(furi_hal_interrupt_irqn[index], NVIC_EncodePriority(NVIC_GetPriorityGrouping(), priority, 0));
    NVIC_EnableIRQ(furi_hal_interrupt_irqn[index]);
}

__attribute__((always_inline)) static inline void furi_hal_interrupt_clear_pending(FuriHalInterruptId index) {
    NVIC_ClearPendingIRQ(furi_hal_interrupt_irqn[index]);
}

__attribute__((always_inline)) static inline void furi_hal_interrupt_get_pending(FuriHalInterruptId index) {
    NVIC_GetPendingIRQ(furi_hal_interrupt_irqn[index]);
}

__attribute__((always_inline)) static inline void furi_hal_interrupt_set_pending(FuriHalInterruptId index) {
    NVIC_SetPendingIRQ(furi_hal_interrupt_irqn[index]);
}

__attribute__((always_inline)) static inline void furi_hal_interrupt_disable(FuriHalInterruptId index) {
    NVIC_DisableIRQ(furi_hal_interrupt_irqn[index]);
}

void furi_hal_interrupt_init() {
    // NVIC_SetPriority(TAMP_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 0, 0));
    // NVIC_EnableIRQ(TAMP_IRQn);

    // NVIC_SetPriority(PendSV_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 15, 0));

    // NVIC_SetPriority(FPU_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 15, 0));
    // NVIC_EnableIRQ(FPU_IRQn);

    // LL_SYSCFG_DisableIT_FPU_IOC();
    // LL_SYSCFG_DisableIT_FPU_DZC();
    // LL_SYSCFG_DisableIT_FPU_UFC();
    // LL_SYSCFG_DisableIT_FPU_OFC();
    // LL_SYSCFG_DisableIT_FPU_IDC();
    // LL_SYSCFG_DisableIT_FPU_IXC();

    // LL_HANDLER_EnableFault(LL_HANDLER_FAULT_USG);
    // LL_HANDLER_EnableFault(LL_HANDLER_FAULT_BUS);
    // LL_HANDLER_EnableFault(LL_HANDLER_FAULT_MEM);

    FURI_LOG_I(TAG, "Init OK");
}

void furi_hal_interrupt_set_isr(FuriHalInterruptId index, FuriHalInterruptISR isr, void* context) {
    FuriHalInterruptPriority priority = furi_kernel_is_running() ? FuriHalInterruptPriorityNormal : FuriHalInterruptPriorityKamiSama;
    furi_hal_interrupt_set_isr_ex(index, priority, isr, context);
}

void furi_hal_interrupt_set_isr_ex(FuriHalInterruptId index, FuriHalInterruptPriority priority, FuriHalInterruptISR isr, void* context) {
    furi_check(index < FuriHalInterruptIdMax);
    furi_check((priority >= FuriHalInterruptPriorityLowest && priority <= FuriHalInterruptPriorityHighest) || priority == FuriHalInterruptPriorityKamiSama);

    uint16_t real_priority = FURI_HAL_INTERRUPT_DEFAULT_PRIORITY - priority;

    if(isr) {
        // Pre ISR set
        furi_check(furi_hal_interrupt_isr[index].isr == NULL);
    } else {
        // Pre ISR clear
        furi_hal_interrupt_disable(index);
        furi_hal_interrupt_clear_pending(index);
    }

    furi_hal_interrupt_isr[index].isr = isr;
    furi_hal_interrupt_isr[index].context = context;
    __DMB();

    if(isr) {
        // Post ISR set
        furi_hal_interrupt_clear_pending(index);
        furi_hal_interrupt_enable(index, real_priority);
    } else {
        // Post ISR clear
    }
}

void TIMER0_IRQ_0_IRQHandler() {
    furi_hal_interrupt_call(FuriHalInterruptIdTimer0Irq0);
}
void TIMER0_IRQ_1_IRQHandler() {
    furi_hal_interrupt_call(FuriHalInterruptIdTimer0Irq1);
}
void TIMER0_IRQ_2_IRQHandler() {
    furi_hal_interrupt_call(FuriHalInterruptIdTimer0Irq2);
}
void TIMER0_IRQ_3_IRQHandler() {
    furi_hal_interrupt_call(FuriHalInterruptIdTimer0Irq3);
}
void TIMER1_IRQ_0_IRQHandler() {
    furi_hal_interrupt_call(FuriHalInterruptIdTimer1Irq0);
}
void TIMER1_IRQ_1_IRQHandler() {
    furi_hal_interrupt_call(FuriHalInterruptIdTimer1Irq1);
}
void TIMER1_IRQ_2_IRQHandler() {
    furi_hal_interrupt_call(FuriHalInterruptIdTimer1Irq2);
}
void TIMER1_IRQ_3_IRQHandler() {
    furi_hal_interrupt_call(FuriHalInterruptIdTimer1Irq3);
}
void PWM_IRQ_WRAP_0_IRQHandler() {
    furi_hal_interrupt_call(FuriHalInterruptIdPwmWrap0);
}
void PWM_IRQ_WRAP_1_IRQHandler() {
    furi_hal_interrupt_call(FuriHalInterruptIdPwmWrap1);
}
void DMA_IRQ_0_IRQHandler() {
    furi_hal_interrupt_call(FuriHalInterruptIdDmaChannel0);
}
void DMA_IRQ_1_IRQHandler() {
    furi_hal_interrupt_call(FuriHalInterruptIdDmaChannel1);
}
void DMA_IRQ_2_IRQHandler() {
    furi_hal_interrupt_call(FuriHalInterruptIdDmaChannel2);
}
void DMA_IRQ_3_IRQHandler() {
    furi_hal_interrupt_call(FuriHalInterruptIdDmaChannel3);
}
void USBCTRL_IRQ_IRQHandler() {
    furi_hal_interrupt_call(FuriHalInterruptIdUsbCtrl);
}
void PIO0_IRQ_0_IRQHandler() {
    furi_hal_interrupt_call(FuriHalInterruptIdPio0Irq0);
}
void PIO0_IRQ_1_IRQHandler() {
    furi_hal_interrupt_call(FuriHalInterruptIdPio0Irq1);
}
void PIO1_IRQ_0_IRQHandler() {
    furi_hal_interrupt_call(FuriHalInterruptIdPio1Irq0);
}
void PIO1_IRQ_1_IRQHandler() {
    furi_hal_interrupt_call(FuriHalInterruptIdPio1Irq1);
}
void PIO2_IRQ_0_IRQHandler() {
    furi_hal_interrupt_call(FuriHalInterruptIdPio2Irq0);
}
void PIO2_IRQ_1_IRQHandler() {
    furi_hal_interrupt_call(FuriHalInterruptIdPio2Irq1);
}
void IO_IRQ_BANK0_IRQHandler() {
    furi_hal_interrupt_call(FuriHalInterruptIdIoBank0);
}
void IO_IRQ_BANK0_NS_IRQHandler() {
    furi_hal_interrupt_call(FuriHalInterruptIdIoBank0Ns);
}
void IO_IRQ_QSPI_IRQHandler() {
    furi_hal_interrupt_call(FuriHalInterruptIdIoQspi);
}
void IO_IRQ_QSPI_NS_IRQHandler() {
    furi_hal_interrupt_call(FuriHalInterruptIdIoQspiNs);
}
void SIO_IRQ_FIFO_IRQHandler() {
    furi_hal_interrupt_call(FuriHalInterruptIdSioFifo);
}
void SIO_IRQ_BELL_IRQHandler() {
    furi_hal_interrupt_call(FuriHalInterruptIdSioBell);
}
void SIO_IRQ_FIFO_NS_IRQHandler() {
    furi_hal_interrupt_call(FuriHalInterruptIdSioFifoNs);
}
void SIO_IRQ_BELL_NS_IRQHandler() {
    furi_hal_interrupt_call(FuriHalInterruptIdSioBellNs);
}
void SIO_IRQ_MTIMECMP_IRQHandler() {
    furi_hal_interrupt_call(FuriHalInterruptIdSioMtimecmp);
}
void CLOCKS_IRQ_IRQHandler() {
    furi_hal_interrupt_call(FuriHalInterruptIdClocks);
}
void SPI0_IRQ_IRQHandler() {
    furi_hal_interrupt_call(FuriHalInterruptIdSpi0);
}
void SPI1_IRQ_IRQHandler() {
    furi_hal_interrupt_call(FuriHalInterruptIdSpi1);
}
void UART0_IRQ_IRQHandler() {
    furi_hal_interrupt_call(FuriHalInterruptIdUart0);
}
void UART1_IRQ_IRQHandler() {
    furi_hal_interrupt_call(FuriHalInterruptIdUart1);
}
void ADC_IRQ_FIFO_IRQHandler() {
    furi_hal_interrupt_call(FuriHalInterruptIdAdcFifo);
}
void I2C0_IRQ_IRQHandler() {
    furi_hal_interrupt_call(FuriHalInterruptIdI2c0);
}
void I2C1_IRQ_IRQHandler() {
    furi_hal_interrupt_call(FuriHalInterruptIdI2c1);
}
void OTP_IRQ_IRQHandler() {
    furi_hal_interrupt_call(FuriHalInterruptIdOtp);
}
void TRNG_IRQ_IRQHandler() {
    furi_hal_interrupt_call(FuriHalInterruptIdTrng);
}
void PROC0_IRQ_CTI_IRQHandler() {
    furi_hal_interrupt_call(FuriHalInterruptIdProc0Cti);
}
void PROC1_IRQ_CTI_IRQHandler() {
    furi_hal_interrupt_call(FuriHalInterruptIdProc1Cti);
}
void PLL_SYS_IRQ_IRQHandler() {
    furi_hal_interrupt_call(FuriHalInterruptIdPllSys);
}
void PLL_USB_IRQ_IRQHandler() {
    furi_hal_interrupt_call(FuriHalInterruptIdPllUsb);
}
void POWMAN_IRQ_POW_IRQHandler() {
    furi_hal_interrupt_call(FuriHalInterruptIdPowmanPow);
}
void POWMAN_IRQ_TIMER_IRQHandler() {
    furi_hal_interrupt_call(FuriHalInterruptIdPowmanTimer);
}
void SPARE_IRQ_0_IRQHandler() {
    furi_hal_interrupt_call(FuriHalInterruptIdSpareIrq0);
}
void SPARE_IRQ_1_IRQHandler() {
    furi_hal_interrupt_call(FuriHalInterruptIdSpareIrq1);
}
void SPARE_IRQ_2_IRQHandler() {
    furi_hal_interrupt_call(FuriHalInterruptIdSpareIrq2);
}
void SPARE_IRQ_3_IRQHandler() {
    furi_hal_interrupt_call(FuriHalInterruptIdSpareIrq3);
}
void SPARE_IRQ_4_IRQHandler() {
    furi_hal_interrupt_call(FuriHalInterruptIdSpareIrq4);
}
void SPARE_IRQ_5_IRQHandler() {
    furi_hal_interrupt_call(FuriHalInterruptIdSpareIrq5);
}

// void NMI_Handler() {
//     if(LL_RCC_IsActiveFlag_HSECSS()) {
//         LL_RCC_ClearFlag_HSECSS();
//         FURI_LOG_E(TAG, "HSE CSS fired: resetting system");
//         NVIC_SystemReset();
//     }
// }

// void HardFault_Handler() {
//     furi_crash("HardFault");
// }

// void MemManage_Handler() {
//     furi_log_puts("\r\n" _FURI_LOG_CLR_E "Mem fault:\r\n");
//     if(FURI_BIT(SCB->CFSR, SCB_CFSR_MLSPERR_Pos)) {
//         furi_log_puts(" - lazy stacking for exception entry\r\n");
//     }

//     if(FURI_BIT(SCB->CFSR, SCB_CFSR_MSTKERR_Pos)) {
//         furi_log_puts(" - stacking for exception entry\r\n");
//     }

//     if(FURI_BIT(SCB->CFSR, SCB_CFSR_MUNSTKERR_Pos)) {
//         furi_log_puts(" - unstacking for exception return\r\n");
//     }

//     if(FURI_BIT(SCB->CFSR, SCB_CFSR_DACCVIOL_Pos)) {
//         furi_log_puts(" - data access violation\r\n");
//     }

//     if(FURI_BIT(SCB->CFSR, SCB_CFSR_IACCVIOL_Pos)) {
//         furi_log_puts(" - instruction access violation\r\n");
//     }

//     if(FURI_BIT(SCB->CFSR, SCB_CFSR_MMARVALID_Pos)) {
//         uint32_t memfault_address = SCB->MMFAR;
//         furi_log_puts(" -- at 0x");
//         furi_log_puthex32(memfault_address);
//         furi_log_puts("\r\n");

//         if(memfault_address < (1024 * 1024)) {
//             furi_log_puts(" -- NULL pointer dereference");
//         } else {
//             // write or read of MPU region 1 (FuriHalMpuRegionStack)
//             furi_log_puts(" -- MPU fault, possibly stack overflow");
//         }
//     }
//     furi_log_puts(_FURI_LOG_CLR_RESET "\r\n");

//     furi_crash("MemManage");
// }

// void BusFault_Handler() {
//     furi_log_puts("\r\n" _FURI_LOG_CLR_E "Bus fault:\r\n");
//     if(FURI_BIT(SCB->CFSR, SCB_CFSR_LSPERR_Pos)) {
//         furi_log_puts(" - lazy stacking for exception entry\r\n");
//     }

//     if(FURI_BIT(SCB->CFSR, SCB_CFSR_STKERR_Pos)) {
//         furi_log_puts(" - stacking for exception entry\r\n");
//     }

//     if(FURI_BIT(SCB->CFSR, SCB_CFSR_UNSTKERR_Pos)) {
//         furi_log_puts(" - unstacking for exception return\r\n");
//     }

//     if(FURI_BIT(SCB->CFSR, SCB_CFSR_IMPRECISERR_Pos)) {
//         furi_log_puts(" - imprecise data access\r\n");
//     }

//     if(FURI_BIT(SCB->CFSR, SCB_CFSR_PRECISERR_Pos)) {
//         furi_log_puts(" - precise data access\r\n");
//     }

//     if(FURI_BIT(SCB->CFSR, SCB_CFSR_IBUSERR_Pos)) {
//         furi_log_puts(" - instruction\r\n");
//     }

//     if(FURI_BIT(SCB->CFSR, SCB_CFSR_BFARVALID_Pos)) {
//         uint32_t busfault_address = SCB->BFAR;
//         furi_log_puts(" -- at 0x");
//         furi_log_puthex32(busfault_address);
//         furi_log_puts("\r\n");

//         if(busfault_address == (uint32_t)NULL) {
//             furi_log_puts(" -- NULL pointer dereference");
//         }
//     }
//     furi_log_puts(_FURI_LOG_CLR_RESET "\r\n");

//     furi_crash("BusFault");
// }

// void UsageFault_Handler() {
//     furi_log_puts("\r\n" _FURI_LOG_CLR_E "Usage fault\r\n");
//     if(FURI_BIT(SCB->CFSR, SCB_CFSR_DIVBYZERO_Pos)) {
//         furi_log_puts(" - division by zero\r\n");
//     }

//     if(FURI_BIT(SCB->CFSR, SCB_CFSR_UNALIGNED_Pos)) {
//         furi_log_puts(" - unaligned access\r\n");
//     }

//     if(FURI_BIT(SCB->CFSR, SCB_CFSR_STKOF_Pos)) {
//         furi_log_puts(" - stack overflow\r\n");
//     }

//     if(FURI_BIT(SCB->CFSR, SCB_CFSR_NOCP_Pos)) {
//         furi_log_puts(" - no coprocessor\r\n");
//     }

//     if(FURI_BIT(SCB->CFSR, SCB_CFSR_INVPC_Pos)) {
//         furi_log_puts(" - invalid PC\r\n");
//     }

//     if(FURI_BIT(SCB->CFSR, SCB_CFSR_INVSTATE_Pos)) {
//         furi_log_puts(" - invalid state\r\n");
//     }

//     if(FURI_BIT(SCB->CFSR, SCB_CFSR_UNDEFINSTR_Pos)) {
//         furi_log_puts(" - undefined instruction\r\n");
//     }
//     furi_log_puts(_FURI_LOG_CLR_RESET);

//     furi_crash("UsageFault");
// }

// void DebugMon_Handler() {
// }

// void FPU_IRQHandler() {
//     furi_crash("FpuFault");
// }

// void FuriSysTick_Handler(void) {
//     // FURI_HAL_INTERRUPT_ACCOUNT_START();
//     furi_hal_os_tick();
//     // FURI_HAL_INTERRUPT_ACCOUNT_END();
// }

// Potential space-saver for updater build
const char* furi_hal_interrupt_get_name(uint8_t exception_number) {
    int32_t id = (int32_t)exception_number - 16;

    switch(id) {
    case -15:
        return "Reset";
        break;
    case -14:
        return "NMI";
        break;
    case -13:
        return "HardFault";
        break;
    case -12:
        return "MemMgmt";
        break;
    case -11:
        return "BusFault";
        break;
    case -10:
        return "UsageFault";
        break;
    case -9:
        return "SecureFault";
        break;
    case -5:
        return "SVC";
        break;
    case -4:
        return "DebugMon";
        break;
    case -2:
        return "PendSV";
        break;
    case -1:
        return "SysTick";
        break;
    case 0:
        return "TIMER0_IRQ_0";
        break;
    case 1:
        return "TIMER0_IRQ_1";
        break;
    case 2:
        return "TIMER0_IRQ_2";
        break;
    case 3:
        return "TIMER0_IRQ_3";
        break;
    case 4:
        return "TIMER1_IRQ_0";
        break;
    case 5:
        return "TIMER1_IRQ_1";
        break;
    case 6:
        return "TIMER1_IRQ_2";
        break;
    case 7:
        return "TIMER1_IRQ_3";
        break;
    case 8:
        return "PWM_IRQ_WRAP_0";
        break;
    case 9:
        return "PWM_IRQ_WRAP_1";
        break;
    case 10:
        return "DMA_IRQ_0";
        break;
    case 11:
        return "DMA_IRQ_1";
        break;
    case 12:
        return "DMA_IRQ_2";
        break;
    case 13:
        return "DMA_IRQ_3";
        break;
    case 14:
        return "USBCTRL_IRQ";
        break;
    case 15:
        return "PIO0_IRQ_0";
        break;
    case 16:
        return "PIO0_IRQ_1";
        break;
    case 17:
        return "PIO1_IRQ_0";
        break;
    case 18:
        return "PIO1_IRQ_1";
        break;
    case 19:
        return "PIO2_IRQ_0";
        break;
    case 20:
        return "PIO2_IRQ_1";
        break;
    case 21:
        return "IO_IRQ_BANK0";
        break;
    case 22:
        return "IO_IRQ_BANK0_NS";
        break;
    case 23:
        return "IO_IRQ_QSPI";
        break;
    case 24:
        return "IO_IRQ_QSPI_NS";
        break;
    case 25:
        return "SIO_IRQ_FIFO";
        break;
    case 26:
        return "SIO_IRQ_BELL";
        break;
    case 27:
        return "SIO_IRQ_FIFO_NS";
        break;
    case 28:
        return "SIO_IRQ_BELL_NS";
        break;
    case 29:
        return "SIO_IRQ_MTIMECMP";
        break;
    case 30:
        return "CLOCKS_IRQ";
        break;
    case 31:
        return "SPI0_IRQ";
        break;
    case 32:
        return "SPI1_IRQ";
        break;
    case 33:
        return "UART0_IRQ";
        break;
    case 34:
        return "UART1_IRQ";
        break;
    case 35:
        return "ADC_IRQ_FIFO";
        break;
    case 36:
        return "I2C0_IRQ";
        break;
    case 37:
        return "I2C1_IRQ";
        break;
    case 38:
        return "OTP_IRQ";
        break;
    case 39:
        return "TRNG_IRQ";
        break;
    case 42:
        return "PLL_SYS_IRQ";
        break;
    case 43:
        return "PLL_USB_IRQ";
        break;
    case 44:
        return "POWMAN_IRQ_POW";
        break;
    case 45:
        return "POWMAN_IRQ_TIMER";
        break;
    default:
        return NULL;
        break;
    }
    return NULL;
}

uint32_t furi_hal_interrupt_get_time_in_isr_total(void) {
    // return furi_hal_interrupt.counter_time_in_isr_total; // TODO
    return 0;
}

void furi_hal_interrupt_assert_valid_priority(void) {
    uint32_t ulCurrentInterrupt = __get_IPSR();

    const uint32_t exti_priority = NVIC_GetPriority(ulCurrentInterrupt - 16);
    uint32_t group_priority, sub_priority;
    NVIC_DecodePriority(exti_priority, NVIC_GetPriorityGrouping(), &group_priority, &sub_priority);

    furi_check(group_priority >= configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
}
