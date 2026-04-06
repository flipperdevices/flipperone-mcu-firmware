#pragma once

#ifdef __cplusplus
extern "C" {
#endif

enum hook_type {
    /*
	 * System initialization.
	 *
	 * Hook routines are called from main(), after all hard-coded inits,
	 * before task scheduling is enabled.
	 */
    HOOK_INIT = 0,

    /*
	 * This hook is called before HOOK_INIT and some early init routines.
	 * Hook routines of this type are expected to be called multiple times.
	 * So, make sure your routine takes care of 'initialized' state.
	 */
    HOOK_INIT_EARLY,

    /*
	 * System clock changed frequency.
	 *
	 * The "pre" frequency hook is called before we change the frequency.
	 * There is no way to cancel.  Hook routines are always called from
	 * a task, so it's OK to lock a mutex here.  However, they may be called
	 * from a deferred task on some platforms so callbacks must make sure
	 * not to do anything that would require some other deferred task to
	 * run.
	 */
    HOOK_PRE_FREQ_CHANGE,
    HOOK_FREQ_CHANGE,

    /*
	 * About to jump to another image.  Modules which need to preserve data
	 * across such a jump should save it here and restore it in HOOK_INIT.
	 *
	 * Hook routines are called from the context which initiates the jump,
	 * WITH INTERRUPTS DISABLED.
	 */
    HOOK_SYSJUMP,

    /*
	 * Initialization for components such as PMU to be done before host
	 * chipset/AP starts up.
	 *
	 * Hook routines are called from the chipset task.
	 */
    HOOK_CHIPSET_PRE_INIT,

    /* System is starting up.  All suspend rails are now on.
	 *
	 * Hook routines are called from the chipset task.
	 */
    HOOK_CHIPSET_STARTUP,

    /*
	 * System is resuming from suspend, or booting and has reached the
	 * point where all voltage rails are on.
	 *
	 * Hook routines are called from the chipset task.
	 */
    HOOK_CHIPSET_RESUME,

    /*
	 * System is suspending, or shutting down; all voltage rails are still
	 * on.
	 *
	 * Hook routines are called from the chipset task.
	 */
    HOOK_CHIPSET_SUSPEND,

#ifdef CONFIG_CHIPSET_RESUME_INIT_HOOK
    /*
	 * Initialization before the system resumes, like enabling the SPI
	 * driver such that it can receive a host resume event.
	 *
	 * Hook routines are called from the chipset task.
	 */
    HOOK_CHIPSET_RESUME_INIT,

    /*
	 * System has suspended. It is paired with CHIPSET_RESUME_INIT hook,
	 * like reverting the initialization of the SPI driver.
	 *
	 * Hook routines are called from the chipset task.
	 */
    HOOK_CHIPSET_SUSPEND_COMPLETE,
#endif

    /*
	 * System is shutting down.  All suspend rails are still on.
	 *
	 * Hook routines are called from the chipset task.
	 */
    HOOK_CHIPSET_SHUTDOWN,

    /*
	 * System has already shut down. All the suspend rails are already off.
	 *
	 * Hook routines are called from the chipset task.
	 */
    HOOK_CHIPSET_SHUTDOWN_COMPLETE,

    /*
	 * System is in G3.  All power rails are now turned off.
	 *
	 * Hook routines are called from the chipset task.
	 */
    HOOK_CHIPSET_HARD_OFF,

    /*
	 * System reset in S0.  All rails are still up.
	 *
	 * Hook routines are called from the chipset task.
	 */
    HOOK_CHIPSET_RESET,

    /*
	 * AC power plugged in or removed.
	 *
	 * Hook routines are called from the TICK task.
	 */
    HOOK_AC_CHANGE,

    /*
	 * Lid opened or closed.  Based on debounced lid state, not raw lid
	 * GPIO input.
	 *
	 * Hook routines are called from the TICK task.
	 */
    HOOK_LID_CHANGE,

    /*
	 * Device in tablet mode (base behind lid).
	 *
	 * Hook routines are called from the TICK task.
	 */
    HOOK_TABLET_MODE_CHANGE,

#if defined(CONFIG_BODY_DETECTION) || defined(CONFIG_PLATFORM_EC_DSP_REMOTE_BODY_DETECTION)
    /*
	 * Body dectection mode change.
	 *
	 * Hook routines are called from the HOOKS task.
	 */
    HOOK_BODY_DETECT_CHANGE,
#endif

    /*
	 * Detachable device connected to a base.
	 *
	 * Hook routines are called from the TICK task.
	 */
    HOOK_BASE_ATTACHED_CHANGE,

    /*
	 * Power button pressed or released.  Based on debounced power button
	 * state, not raw GPIO input.
	 *
	 * Hook routines are called from the TICK task.
	 */
    HOOK_POWER_BUTTON_CHANGE,

    /*
	 * Battery state-of-charge changed
	 *
	 * Hook routines are called from the charger task.
	 */
    HOOK_BATTERY_SOC_CHANGE,

#ifdef CONFIG_USB_SUSPEND
    /*
	 * Called when there is a change in USB power management status
	 * (suspended or resumed).
	 *
	 * Hook routines are called from HOOKS task.
	 */
    HOOK_USB_PM_CHANGE,
#endif

    /*
	 * Periodic tick, every HOOK_TICK_INTERVAL.
	 *
	 * Hook routines will be called from the TICK task.
	 */
    HOOK_TICK,

#if !defined(CONFIG_ZEPHYR) || defined(CONFIG_PLATFORM_EC_HOOK_SECOND)
    /*
	 * Periodic tick, every second.
	 *
	 * Hook routines will be called from the TICK task.
	 */
    HOOK_SECOND,
#endif

    /*
	 * USB PD cc disconnect event.
	 */
    HOOK_USB_PD_DISCONNECT,

    /*
	 * USB PD cc connection event.
	 */
    HOOK_USB_PD_CONNECT,

    /*
	 * Power supply change event.
	 */
    HOOK_POWER_SUPPLY_CHANGE,

#ifdef TEST_BUILD
    /*
	 * Special hook types to be used by unit tests of the hooks
	 * implementation only.
	 */
    HOOK_TEST_1,
    HOOK_TEST_2,
    HOOK_TEST_3,
#endif /* TEST_BUILD */

    /*
	 * Not a hook type (instead the number of hooks). This should
	 * always be placed at the end of this enumeration.
	 */
    HOOK_TYPE_COUNT,
};

/**
 * Call all the hook routines of a specified type.
 *
 * This function must be called from the correct type-specific context (task);
 * see enum hook_type for details.  hook_notify() should NEVER be called from
 * interrupt context unless specifically allowed for a hook type, because hook
 * routines may need to perform task-level calls like crec_usleep() and mutex
 * operations that are not valid in interrupt context.  Instead of calling a
 * hook from interrupt context, use a deferred function.
 *
 * @param type		Type of hook routines to call.
 */
void hook_notify(enum hook_type type);

#ifdef __cplusplus
}
#endif
