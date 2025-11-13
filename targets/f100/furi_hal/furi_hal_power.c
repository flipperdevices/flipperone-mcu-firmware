#include "core/check.h"
#include "core/kernel.h"
#include "core/log.h"
#include "furi_hal_serial.h"
#include <furi.h>
#include <furi_hal.h>
#include <hardware/uart.h>
#include <hardware/watchdog.h>
#include <pico/types.h>
#include <stdint.h>
#include <sys/_intsup.h>

#define TAG "FuriHalPower"

typedef struct {
    volatile uint8_t insomnia;
} FuriHalPower;

static volatile FuriHalPower furi_hal_power = {
    .insomnia = 0,
};

void furi_hal_power_reset(void) {
    watchdog_reboot(0, 0, 10);
}

uint16_t furi_hal_power_insomnia_level(void) {
    return furi_hal_power.insomnia;
}

void furi_hal_power_insomnia_enter(void) {
    FURI_CRITICAL_ENTER();
    furi_check(furi_hal_power.insomnia < UINT8_MAX);
    furi_hal_power.insomnia++;
    FURI_CRITICAL_EXIT();
}

void furi_hal_power_insomnia_exit(void) {
    FURI_CRITICAL_ENTER();
    furi_check(furi_hal_power.insomnia > 0);
    furi_hal_power.insomnia--;
    FURI_CRITICAL_EXIT();
}

bool furi_hal_power_sleep_available(void) {
    return furi_hal_power.insomnia == 0;
}

//####################
#include "hardware/xosc.h"
#include "hardware/structs/rosc.h"
#include "hardware/clocks.h"

#include "hardware/pll.h"

#include "hardware/powman.h"
#include "hardware/sync.h"
#include "pico/runtime_init.h"

#include "pico/platform/cpu_regs.h"

static bool awake;
int alarm_num;
#define FURI_HAL_OS_TICK_HZ        configTICK_RATE_HZ

#define FURI_HAL_OS_IDLE_CNT_TO_TICKS(x) (((x) * FURI_HAL_OS_TICK_HZ) / FURI_HAL_IDLE_TIMER_CLK_HZ)
#define FURI_HAL_OS_TICKS_TO_IDLE_CNT(x) (((x) * FURI_HAL_IDLE_TIMER_CLK_HZ) / FURI_HAL_OS_TICK_HZ)


static void alarm_sleep_callback(uint alarm_id) {
    // printf("alarm woke us up\n");
    // uart_default_tx_wait_blocking();
    awake = true;
    hardware_alarm_set_callback(alarm_id, NULL);
    hardware_alarm_unclaim(alarm_id);
}


typedef enum {
    DORMANT_SOURCE_NONE,
    DORMANT_SOURCE_XOSC,
    DORMANT_SOURCE_ROSC,
    DORMANT_SOURCE_LPOSC, // rp2350 only
} dormant_source_t;

static dormant_source_t _dormant_source;

inline static void rosc_clear_bad_write(void) {
    hw_clear_bits(&rosc_hw->status, ROSC_STATUS_BADWRITE_BITS);
}

inline static void rosc_write(io_rw_32 *addr, uint32_t value) {
    rosc_clear_bad_write();
    assert(rosc_write_okay());
    *addr = value;
    assert(rosc_write_okay());
};


void rosc_disable(void) {
    uint32_t tmp = rosc_hw->ctrl;
    tmp &= (~ROSC_CTRL_ENABLE_BITS);
    tmp |= (ROSC_CTRL_ENABLE_VALUE_DISABLE << ROSC_CTRL_ENABLE_LSB);
    rosc_write(&rosc_hw->ctrl, tmp);
    // Wait for stable to go away
    while(rosc_hw->status & ROSC_STATUS_STABLE_BITS);
}

void rosc_enable(void) {
    //Re-enable the rosc
    rosc_write(&rosc_hw->ctrl, ROSC_CTRL_ENABLE_BITS);

    //Wait for it to become stable once restarted
    while (!(rosc_hw->status & ROSC_STATUS_STABLE_BITS));
}

// In order to go into dormant mode we need to be running from a stoppable clock source:
// either the xosc or rosc with no PLLs running. This means we disable the USB and ADC clocks
// and all PLLs
void sleep_run_from_dormant_source(dormant_source_t dormant_source) {
    assert(dormant_source_valid(dormant_source));
    _dormant_source = dormant_source;

    uint src_hz;
    uint clk_ref_src;
    switch (dormant_source) {
        case DORMANT_SOURCE_XOSC:
            src_hz = XOSC_HZ;
            clk_ref_src = CLOCKS_CLK_REF_CTRL_SRC_VALUE_XOSC_CLKSRC;
            break;
        case DORMANT_SOURCE_ROSC:
            src_hz = 6500 * KHZ; // todo
            clk_ref_src = CLOCKS_CLK_REF_CTRL_SRC_VALUE_ROSC_CLKSRC_PH;
            break;
#if !PICO_RP2040
        case DORMANT_SOURCE_LPOSC:
            src_hz = 32 * KHZ;
            clk_ref_src = CLOCKS_CLK_REF_CTRL_SRC_VALUE_LPOSC_CLKSRC;
            break;
#endif
        default:
            hard_assert(false);
    }

    // CLK_REF = XOSC or ROSC
    clock_configure(clk_ref,
                    clk_ref_src,
                    0, // No aux mux
                    src_hz,
                    src_hz);

    // CLK SYS = CLK_REF
    clock_configure(clk_sys,
                    CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLK_REF,
                    0, // Using glitchless mux
                    src_hz,
                    src_hz);

    // CLK ADC = 0MHz
    clock_stop(clk_adc);
    clock_stop(clk_usb);
#if PICO_RP2350
    clock_stop(clk_hstx);
#endif

#if PICO_RP2040
    // CLK RTC = ideally XOSC (12MHz) / 256 = 46875Hz but could be rosc
    uint clk_rtc_src = (dormant_source == DORMANT_SOURCE_XOSC) ?
                       CLOCKS_CLK_RTC_CTRL_AUXSRC_VALUE_XOSC_CLKSRC :
                       CLOCKS_CLK_RTC_CTRL_AUXSRC_VALUE_ROSC_CLKSRC_PH;

    clock_configure(clk_rtc,
                    0, // No GLMUX
                    clk_rtc_src,
                    src_hz,
                    46875);
#endif

    // CLK PERI = clk_sys. Used as reference clock for Peripherals. No dividers so just select and enable
    clock_configure(clk_peri,
                    0,
                    CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLK_SYS,
                    src_hz,
                    src_hz);

    pll_deinit(pll_sys);
    pll_deinit(pll_usb);

    // Assuming both xosc and rosc are running at the moment
    if (dormant_source == DORMANT_SOURCE_XOSC) {
        // Can disable rosc
        rosc_disable();
    } else {
        // Can disable xosc
        xosc_disable();
    }

    // Reconfigure uart with new clocks
    //setup_default_uart();
}

static void processor_deep_sleep(void) {
    // Enable deep sleep at the proc
#ifdef __riscv
    uint32_t bits = RVCSR_MSLEEP_POWERDOWN_BITS;
    if (!get_core_num()) {
        bits |= RVCSR_MSLEEP_DEEPSLEEP_BITS;
    }
    riscv_set_csr(RVCSR_MSLEEP_OFFSET, bits);
#else
    scb_hw->scr |= ARM_CPU_PREFIXED(SCR_SLEEPDEEP_BITS);
#endif
}

bool sleep_goto_sleep_for(uint32_t delay_ms, hardware_alarm_callback_t callback)
{
    // We should have already called the sleep_run_from_dormant_source function
    // This is only needed for dormancy although it saves power running from xosc while sleeping
    //assert(dormant_source_valid(_dormant_source));

    // Turn off all clocks except for the timer
    clocks_hw->sleep_en0 = 0x0;
#if PICO_RP2040
    clocks_hw->sleep_en1 = CLOCKS_SLEEP_EN1_CLK_SYS_TIMER_BITS;
#elif PICO_RP2350
    clocks_hw->sleep_en1 = CLOCKS_SLEEP_EN1_CLK_REF_TICKS_BITS | CLOCKS_SLEEP_EN1_CLK_SYS_TIMER0_BITS;
#else
#error Unknown processor
#endif

    alarm_num = hardware_alarm_claim_unused(true);
    hardware_alarm_set_callback(alarm_num, callback);
    absolute_time_t t = make_timeout_time_ms(delay_ms);
   // uint64_t delay_us = (uint64_t)delay_ms * 1000ull;
    //delay_us = delay_us / 4;
   // absolute_time_t t = make_timeout_time_us(delay_us);
    if (hardware_alarm_set_target(alarm_num, t)) {
        hardware_alarm_set_callback(alarm_num, NULL);
        hardware_alarm_unclaim(alarm_num);
        return false;
    }

    //stdio_flush();

    // Enable deep sleep at the proc
    processor_deep_sleep();

    // Go to sleep
    __wfi();
    return true;
}

// To be called after waking up from sleep/dormant mode to restore system clocks properly
void sleep_power_up(void)
{
    // Re-enable the ring oscillator, which will essentially kickstart the proc
    rosc_enable();

    // Reset the sleep enable register so peripherals and other hardware can be used
    clocks_hw->sleep_en0 |= ~(0u);
    clocks_hw->sleep_en1 |= ~(0u);

    // Restore all clocks
    clocks_init();

#if PICO_RP2350
    // make powerman use xosc again
    uint64_t restore_ms = powman_timer_get_ms();
    powman_timer_set_1khz_tick_source_xosc();
    powman_timer_set_ms(restore_ms);
#endif

    // UART needs to be reinitialised with the new clock frequencies for stable output
    //setup_default_uart();
    if(hardware_alarm_is_claimed(alarm_num)){
        hardware_alarm_set_callback(alarm_num, NULL);
        hardware_alarm_unclaim(alarm_num);
    }
}

uint32_t furi_hal_power_sleep(uint32_t expected_idle_ticks) {

    // stdio_init_all();
    // printf("Hello Alarm Sleep!\n");
    //sleep_ms(1000 * 10);
   // uint8_t alarm_num = 0;

        //printf("Awake for 10 seconds\n");
       // sleep_ms(1000);
       // alarm_num++;
       // printf("Alarm %d triggered\n", alarm_num);

       // printf("Switching to XOSC\n");

        // Wait for the fifo to be drained so we get reliable output
        //uart_default_tx_wait_blocking();

        // Set the crystal oscillator as the dormant clock source, UART will be reconfigured from here
        // This is only really necessary before sending the pico dormant but running from xosc while asleep saves power
        uint64_t time_start = time_us_64();

        sleep_run_from_dormant_source(DORMANT_SOURCE_XOSC);

        awake = false;

        // Go to sleep until the alarm interrupt is generated after 10 seconds
        //printf("Sleeping for 10 seconds\n");
        //uart_default_tx_wait_blocking();
        //uint32_t sleep_ms = FURI_HAL_OS_TICKS_TO_IDLE_CNT(expected_idle_ticks) * 1000 / FURI_HAL_OS_TICK_HZ;
        

        if (sleep_goto_sleep_for(expected_idle_ticks, &alarm_sleep_callback)) {
            // Make sure we don't wake
            // while (!awake) {
            //     //printf("Should be sleeping\n");
            // }
        }

        // Re-enabling clock sources and generators.
        sleep_power_up();

        return (uint32_t)(time_us_64() - time_start) / 1000;

}