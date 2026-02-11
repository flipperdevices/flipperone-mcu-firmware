#include <furi_hal.h>
#include <FreeRTOS.h>
#include <hardware/structs/resets.h>
#include <hardware/irq.h>

#define TAG "FuriHalInterrupt"

#define FURI_HAL_INTERRUPT_DEFAULT_PRIORITY (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY + 5)

typedef struct {
    FuriHalInterruptISR isr;
    void* context;
} FuriHalInterruptISRPair;

FuriHalInterruptISRPair furi_hal_interrupt_isr[FuriHalInterruptIdMax] = {0};

const IRQn_Type furi_hal_interrupt_irqn[FuriHalInterruptIdMax] = {
    [FuriHalInterruptIdTimer0Irq0] = (IRQn_Type)TIMER0_IRQ_0,
    [FuriHalInterruptIdTimer0Irq1] = (IRQn_Type)TIMER0_IRQ_1,
    [FuriHalInterruptIdTimer0Irq2] = (IRQn_Type)TIMER0_IRQ_2,
    [FuriHalInterruptIdTimer0Irq3] = (IRQn_Type)TIMER0_IRQ_3,
    [FuriHalInterruptIdTimer1Irq0] = (IRQn_Type)TIMER1_IRQ_0,
    [FuriHalInterruptIdTimer1Irq1] = (IRQn_Type)TIMER1_IRQ_1,
    [FuriHalInterruptIdTimer1Irq2] = (IRQn_Type)TIMER1_IRQ_2,
    [FuriHalInterruptIdTimer1Irq3] = (IRQn_Type)TIMER1_IRQ_3,
    [FuriHalInterruptIdPwmWrap0] = (IRQn_Type)PWM_IRQ_WRAP_0,
    [FuriHalInterruptIdPwmWrap1] = (IRQn_Type)PWM_IRQ_WRAP_1,
    [FuriHalInterruptIdDmaChannel0] = (IRQn_Type)DMA_IRQ_0,
    [FuriHalInterruptIdDmaChannel1] = (IRQn_Type)DMA_IRQ_1,
    [FuriHalInterruptIdDmaChannel2] = (IRQn_Type)DMA_IRQ_2,
    [FuriHalInterruptIdDmaChannel3] = (IRQn_Type)DMA_IRQ_3,
    [FuriHalInterruptIdUsbCtrl] = (IRQn_Type)USBCTRL_IRQ,
    [FuriHalInterruptIdPio0Irq0] = (IRQn_Type)PIO0_IRQ_0,
    [FuriHalInterruptIdPio0Irq1] = (IRQn_Type)PIO0_IRQ_1,
    [FuriHalInterruptIdPio1Irq0] = (IRQn_Type)PIO1_IRQ_0,
    [FuriHalInterruptIdPio1Irq1] = (IRQn_Type)PIO1_IRQ_1,
    [FuriHalInterruptIdPio2Irq0] = (IRQn_Type)PIO2_IRQ_0,
    [FuriHalInterruptIdPio2Irq1] = (IRQn_Type)PIO2_IRQ_1,
    [FuriHalInterruptIdIoBank0] = (IRQn_Type)IO_IRQ_BANK0,
    [FuriHalInterruptIdIoBank0Ns] = (IRQn_Type)IO_IRQ_BANK0_NS,
    [FuriHalInterruptIdIoQspi] = (IRQn_Type)IO_IRQ_QSPI,
    [FuriHalInterruptIdIoQspiNs] = (IRQn_Type)IO_IRQ_QSPI_NS,
    [FuriHalInterruptIdSioFifo] = (IRQn_Type)SIO_IRQ_FIFO,
    [FuriHalInterruptIdSioBell] = (IRQn_Type)SIO_IRQ_BELL,
    [FuriHalInterruptIdSioFifoNs] = (IRQn_Type)SIO_IRQ_FIFO_NS,
    [FuriHalInterruptIdSioBellNs] = (IRQn_Type)SIO_IRQ_BELL_NS,
    [FuriHalInterruptIdSioMtimecmp] = (IRQn_Type)SIO_IRQ_MTIMECMP,
    [FuriHalInterruptIdClocks] = (IRQn_Type)CLOCKS_IRQ,
    [FuriHalInterruptIdSpi0] = (IRQn_Type)SPI0_IRQ,
    [FuriHalInterruptIdSpi1] = (IRQn_Type)SPI1_IRQ,
    [FuriHalInterruptIdUart0] = (IRQn_Type)UART0_IRQ,
    [FuriHalInterruptIdUart1] = (IRQn_Type)UART1_IRQ,
    [FuriHalInterruptIdAdcFifo] = (IRQn_Type)ADC_IRQ_FIFO,
    [FuriHalInterruptIdI2c0] = (IRQn_Type)I2C0_IRQ,
    [FuriHalInterruptIdI2c1] = (IRQn_Type)I2C1_IRQ,
    [FuriHalInterruptIdOtp] = (IRQn_Type)OTP_IRQ,
    [FuriHalInterruptIdTrng] = (IRQn_Type)TRNG_IRQ,
    [FuriHalInterruptIdProc0Cti] = (IRQn_Type)PROC0_IRQ_CTI,
    [FuriHalInterruptIdProc1Cti] = (IRQn_Type)PROC1_IRQ_CTI,
    [FuriHalInterruptIdPllSys] = (IRQn_Type)PLL_SYS_IRQ,
    [FuriHalInterruptIdPllUsb] = (IRQn_Type)PLL_USB_IRQ,
    [FuriHalInterruptIdPowmanPow] = (IRQn_Type)POWMAN_IRQ_POW,
    [FuriHalInterruptIdPowmanTimer] = (IRQn_Type)POWMAN_IRQ_TIMER,
    [FuriHalInterruptIdSpareIrq0] = (IRQn_Type)SPARE_IRQ_0,
    [FuriHalInterruptIdSpareIrq1] = (IRQn_Type)SPARE_IRQ_1,
    [FuriHalInterruptIdSpareIrq2] = (IRQn_Type)SPARE_IRQ_2,
    [FuriHalInterruptIdSpareIrq3] = (IRQn_Type)SPARE_IRQ_3,
    [FuriHalInterruptIdSpareIrq4] = (IRQn_Type)SPARE_IRQ_4,
    [FuriHalInterruptIdSpareIrq5] = (IRQn_Type)SPARE_IRQ_5,
};

__attribute__((always_inline)) static inline void furi_hal_interrupt_call(FuriHalInterruptId index) {
    furi_check(furi_hal_interrupt_isr[index].isr);
    furi_hal_interrupt_isr[index].isr(furi_hal_interrupt_isr[index].context);
}

// __attribute__((always_inline)) static inline void furi_hal_interrupt_enable(FuriHalInterruptId index, uint16_t priority) {
//     NVIC_SetPriority(furi_hal_interrupt_irqn[index], NVIC_EncodePriority(NVIC_GetPriorityGrouping(), priority, 0));
//     NVIC_EnableIRQ(furi_hal_interrupt_irqn[index]);
// }

// __attribute__((always_inline)) static inline void furi_hal_interrupt_clear_pending(FuriHalInterruptId index) {
//     NVIC_ClearPendingIRQ(furi_hal_interrupt_irqn[index]);
// }

// __attribute__((always_inline)) static inline void furi_hal_interrupt_get_pending(FuriHalInterruptId index) {
//     NVIC_GetPendingIRQ(furi_hal_interrupt_irqn[index]);
// }

// __attribute__((always_inline)) static inline void furi_hal_interrupt_set_pending(FuriHalInterruptId index) {
//     NVIC_SetPendingIRQ(furi_hal_interrupt_irqn[index]);
// }

// __attribute__((always_inline)) static inline void furi_hal_interrupt_disable(FuriHalInterruptId index) {
//     NVIC_DisableIRQ(furi_hal_interrupt_irqn[index]);
// }

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

void __isr __not_in_flash_func(timer0_irq_0_handler)(void) {
    furi_hal_interrupt_call(FuriHalInterruptIdTimer0Irq0);
}
void __isr __not_in_flash_func(timer0_irq_1_handler)(void) {
    furi_hal_interrupt_call(FuriHalInterruptIdTimer0Irq1);
}
void __isr __not_in_flash_func(timer0_irq_2_handler)(void) {
    furi_hal_interrupt_call(FuriHalInterruptIdTimer0Irq2);
}
void __isr __not_in_flash_func(timer0_irq_3_handler)(void) {
    furi_hal_interrupt_call(FuriHalInterruptIdTimer0Irq3);
}
void __isr __not_in_flash_func(timer1_irq_0_handler)(void) {
    furi_hal_interrupt_call(FuriHalInterruptIdTimer1Irq0);
}
void __isr __not_in_flash_func(timer1_irq_1_handler)(void) {
    furi_hal_interrupt_call(FuriHalInterruptIdTimer1Irq1);
}
void __isr __not_in_flash_func(timer1_irq_2_handler)(void) {
    furi_hal_interrupt_call(FuriHalInterruptIdTimer1Irq2);
}
void __isr __not_in_flash_func(timer1_irq_3_handler)(void) {
    furi_hal_interrupt_call(FuriHalInterruptIdTimer1Irq3);
}
void __isr __not_in_flash_func(pwm_irq_wrap_0_handler)(void) {
    furi_hal_interrupt_call(FuriHalInterruptIdPwmWrap0);
}
void __isr __not_in_flash_func(pwm_irq_wrap_1_handler)(void) {
    furi_hal_interrupt_call(FuriHalInterruptIdPwmWrap1);
}
void __isr __not_in_flash_func(dma_irq_0_handler)(void) {
    furi_hal_interrupt_call(FuriHalInterruptIdDmaChannel0);
}
void __isr __not_in_flash_func(dma_irq_1_handler)(void) {
    furi_hal_interrupt_call(FuriHalInterruptIdDmaChannel1);
}
void __isr __not_in_flash_func(dma_irq_2_handler)(void) {
    furi_hal_interrupt_call(FuriHalInterruptIdDmaChannel2);
}
void __isr __not_in_flash_func(dma_irq_3_handler)(void) {
    furi_hal_interrupt_call(FuriHalInterruptIdDmaChannel3);
}
void __isr __not_in_flash_func(usbctrl_irq_handler)(void) {
    furi_hal_interrupt_call(FuriHalInterruptIdUsbCtrl);
}
void __isr __not_in_flash_func(pio0_irq_0_handler)(void) {
    furi_hal_interrupt_call(FuriHalInterruptIdPio0Irq0);
}
void __isr __not_in_flash_func(pio0_irq_1_handler)(void) {
    furi_hal_interrupt_call(FuriHalInterruptIdPio0Irq1);
}
void __isr __not_in_flash_func(pio1_irq_0_handler)(void) {
    furi_hal_interrupt_call(FuriHalInterruptIdPio1Irq0);
}
void __isr __not_in_flash_func(pio1_irq_1_handler)(void) {
    furi_hal_interrupt_call(FuriHalInterruptIdPio1Irq1);
}
void __isr __not_in_flash_func(pio2_irq_0_handler)(void) {
    furi_hal_interrupt_call(FuriHalInterruptIdPio2Irq0);
}
void __isr __not_in_flash_func(pio2_irq_1_handler)(void) {
    furi_hal_interrupt_call(FuriHalInterruptIdPio2Irq1);
}
void __isr __not_in_flash_func(io_irq_bank0_handler)(void) {
    furi_hal_interrupt_call(FuriHalInterruptIdIoBank0);
}
void __isr __not_in_flash_func(io_irq_bank0_ns_handler)(void) {
    furi_hal_interrupt_call(FuriHalInterruptIdIoBank0Ns);
}
void __isr __not_in_flash_func(io_irq_qspi_handler)(void) {
    furi_hal_interrupt_call(FuriHalInterruptIdIoQspi);
}
void __isr __not_in_flash_func(io_irq_qspi_ns_handler)(void) {
    furi_hal_interrupt_call(FuriHalInterruptIdIoQspiNs);
}
void __isr __not_in_flash_func(sio_irq_fifo_handler)(void) {
    furi_hal_interrupt_call(FuriHalInterruptIdSioFifo);
}
void __isr __not_in_flash_func(sio_irq_bell_handler)(void) {
    furi_hal_interrupt_call(FuriHalInterruptIdSioBell);
}
void __isr __not_in_flash_func(sio_irq_fifo_ns_handler)(void) {
    furi_hal_interrupt_call(FuriHalInterruptIdSioFifoNs);
}
void __isr __not_in_flash_func(sio_irq_bell_ns_handler)(void) {
    furi_hal_interrupt_call(FuriHalInterruptIdSioBellNs);
}
void __isr __not_in_flash_func(sio_irq_mtimecmp_handler)(void) {
    furi_hal_interrupt_call(FuriHalInterruptIdSioMtimecmp);
}
void __isr __not_in_flash_func(clocks_irq_handler)(void) {
    furi_hal_interrupt_call(FuriHalInterruptIdClocks);
}
void __isr __not_in_flash_func(spi0_irq_handler)(void) {
    furi_hal_interrupt_call(FuriHalInterruptIdSpi0);
}
void __isr __not_in_flash_func(spi1_irq_handler)(void) {
    furi_hal_interrupt_call(FuriHalInterruptIdSpi1);
}
void __isr __not_in_flash_func(uart0_irq_handler)(void) {
    furi_hal_interrupt_call(FuriHalInterruptIdUart0);
}
void __isr __not_in_flash_func(uart1_irq_handler)(void) {
    furi_hal_interrupt_call(FuriHalInterruptIdUart1);
}
void __isr __not_in_flash_func(adc_irq_fifo_handler)(void) {
    furi_hal_interrupt_call(FuriHalInterruptIdAdcFifo);
}
void __isr __not_in_flash_func(i2c0_irq_handler)(void) {
    furi_hal_interrupt_call(FuriHalInterruptIdI2c0);
}
void __isr __not_in_flash_func(i2c1_irq_handler)(void) {
    furi_hal_interrupt_call(FuriHalInterruptIdI2c1);
}
void __isr __not_in_flash_func(otp_irq_handler)(void) {
    furi_hal_interrupt_call(FuriHalInterruptIdOtp);
}
void __isr __not_in_flash_func(trng_irq_handler)(void) {
    furi_hal_interrupt_call(FuriHalInterruptIdTrng);
}
void __isr __not_in_flash_func(proc0_cti_handler)(void) {
    furi_hal_interrupt_call(FuriHalInterruptIdProc0Cti);
}
void __isr __not_in_flash_func(proc1_cti_handler)(void) {
    furi_hal_interrupt_call(FuriHalInterruptIdProc1Cti);
}
void __isr __not_in_flash_func(pll_sys_irq_handler)(void) {
    furi_hal_interrupt_call(FuriHalInterruptIdPllSys);
}
void __isr __not_in_flash_func(pll_usb_irq_handler)(void) {
    furi_hal_interrupt_call(FuriHalInterruptIdPllUsb);
}
void __isr __not_in_flash_func(powman_pow_irq_handler)(void) {
    furi_hal_interrupt_call(FuriHalInterruptIdPowmanPow);
}
void __isr __not_in_flash_func(powman_timer_irq_handler)(void) {
    furi_hal_interrupt_call(FuriHalInterruptIdPowmanTimer);
}
void __isr __not_in_flash_func(spare_irq_0_handler)(void) {
    furi_hal_interrupt_call(FuriHalInterruptIdSpareIrq0);
}
void __isr __not_in_flash_func(spare_irq_1_handler)(void) {
    furi_hal_interrupt_call(FuriHalInterruptIdSpareIrq1);
}
void __isr __not_in_flash_func(spare_irq_2_handler)(void) {
    furi_hal_interrupt_call(FuriHalInterruptIdSpareIrq2);
}
void __isr __not_in_flash_func(spare_irq_3_handler)(void) {
    furi_hal_interrupt_call(FuriHalInterruptIdSpareIrq3);
}
void __isr __not_in_flash_func(spare_irq_4_handler)(void) {
    furi_hal_interrupt_call(FuriHalInterruptIdSpareIrq4);
}
void __isr __not_in_flash_func(spare_irq_5_handler)(void) {
    furi_hal_interrupt_call(FuriHalInterruptIdSpareIrq5);
}

//the sequence cannot be changed
void (*furi_hal_interrupt_isr_handler[FuriHalInterruptIdMax])(void) = {
    timer0_irq_0_handler,   timer0_irq_1_handler, timer0_irq_2_handler,   timer0_irq_3_handler,     timer1_irq_0_handler,    timer1_irq_1_handler,
    timer1_irq_2_handler,   timer1_irq_3_handler, pwm_irq_wrap_0_handler, pwm_irq_wrap_1_handler,   dma_irq_0_handler,       dma_irq_1_handler,
    dma_irq_2_handler,      dma_irq_3_handler,    usbctrl_irq_handler,    pio0_irq_0_handler,       pio0_irq_1_handler,      pio1_irq_0_handler,
    pio1_irq_1_handler,     pio2_irq_0_handler,   pio2_irq_1_handler,     io_irq_bank0_handler,     io_irq_bank0_ns_handler, io_irq_qspi_handler,
    io_irq_qspi_ns_handler, sio_irq_fifo_handler, sio_irq_bell_handler,   sio_irq_fifo_ns_handler,  sio_irq_bell_ns_handler, sio_irq_mtimecmp_handler,
    clocks_irq_handler,     spi0_irq_handler,     spi1_irq_handler,       uart0_irq_handler,        uart1_irq_handler,       adc_irq_fifo_handler,
    i2c0_irq_handler,       i2c1_irq_handler,     otp_irq_handler,        trng_irq_handler,         proc0_cti_handler,       proc1_cti_handler,
    pll_sys_irq_handler,    pll_usb_irq_handler,  powman_pow_irq_handler, powman_timer_irq_handler, spare_irq_0_handler,     spare_irq_1_handler,
    spare_irq_2_handler,    spare_irq_3_handler,  spare_irq_4_handler,    spare_irq_5_handler,
};

void furi_hal_interrupt_set_isr_ex(FuriHalInterruptId index, FuriHalInterruptPriority priority, FuriHalInterruptISR isr, void* context) {
    furi_check(index < FuriHalInterruptIdMax);
    furi_check((priority >= FuriHalInterruptPriorityLowest && priority <= FuriHalInterruptPriorityHighest) || priority == FuriHalInterruptPriorityKamiSama);

    uint16_t real_priority = FURI_HAL_INTERRUPT_DEFAULT_PRIORITY - priority;

    if(isr) {
        // Pre ISR set
        furi_check(furi_hal_interrupt_isr[index].isr == NULL);
    } else {
        // Pre ISR clear

        //Todo: check the implementation
        irq_set_enabled(furi_hal_interrupt_irqn[index], false);
        irq_set_exclusive_handler(furi_hal_interrupt_irqn[index], NULL);
        // furi_hal_interrupt_disable(index);
        // furi_hal_interrupt_clear_pending(index);
    }

    furi_hal_interrupt_isr[index].isr = isr;
    furi_hal_interrupt_isr[index].context = context;
    __DMB();

    if(isr) {
        // Post ISR set
        // furi_hal_interrupt_clear_pending(index);
        // furi_hal_interrupt_enable(index, real_priority);

        //Todo: check the implementation
        irq_set_exclusive_handler(furi_hal_interrupt_irqn[index], furi_hal_interrupt_isr_handler[index]);
        irq_set_priority(furi_hal_interrupt_irqn[index], real_priority);
        irq_set_enabled(furi_hal_interrupt_irqn[index], true);
    } else {
        // Post ISR clear
    }
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
