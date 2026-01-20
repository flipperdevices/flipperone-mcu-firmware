#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/** Disable the ring oscillator */
void furi_hal_clock_rosc_disable(void);

/** Enable the ring oscillator */
void furi_hal_clock_rosc_enable(void);

/** Early initialization */
void furi_hal_clock_init_early(void);

/** Early deinitialization */
void furi_hal_clock_deinit_early(void);

/** Initialize clocks */
void furi_hal_clock_init(void);

/** Stop SysTick counter without resetting */
void furi_hal_clock_suspend_tick(void);

/** Continue SysTick counter operation */
void furi_hal_clock_resume_tick(void);

#ifdef __cplusplus
}
#endif
