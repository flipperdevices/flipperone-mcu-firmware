#include "host.h"

#include <api_lock.h>
#include <desktop/desktop.h>
#include <power/power.h>
#include <updater/updater.h>
#include <pd/pd.h>
#include <furi_bsp_linux.h>
#include <furi_hal_power.h>
#include <pico/bootrom.h>
#include <boot/bootrom_constants.h>
#include <hardware/structs/watchdog.h>
#include <i2c_intercom/i2c_registers.h>
#include <i2c_intercom/i2c_registers_map.h>

#define TAG "Host"

#define HOST_MAX_MESSAGES          (8)
#define HOST_POWER_OFF_DELAY_MS    (300)
#define HOST_MASKROM_RELEASE_MS    (1000)
#define HOST_RESET_ATTEMPTS        (3) // charger power-reset tries before giving up
#define HOST_CPU_APP_RETRY_MS      (1000) // wait for self_check to release the desktop

struct Host {
    FuriEventLoop* event_loop;
    FuriMessageQueue* message_queue;
    CpuState cpu_state;
    FuriEventLoopTimer* shutdown_timer;
    FuriEventLoopTimer* cpu_app_retry_timer;
    FuriEventLoopTimer* maskrom_timer;
    FuriEventLoopTimer* reset_timer;
    HostResetResponse reset_response;
    uint16_t reset_delay_100ms;
    uint32_t dropped_messages_logged;
    Power* power;
    Updater* updater;
};

typedef enum {
    HostMessageTypeSetCpuState,
    HostMessageTypePowerOn,
    HostMessageTypePowerOff,
    HostMessageTypePowerRestart,
    HostMessageTypePowerMaskrom,
    HostMessageTypeSetResetResponse,
    HostMessageTypeSetResetDelay,
    HostMessageTypeI2cSystemReset,
} HostMessageType;

typedef struct {
    HostMessageType type;
    FuriApiLock lock;
    bool* result;
    union {
        CpuState cpu_state;
        HostResetResponse reset_response;
        uint16_t reset_delay_100ms;
        uint16_t i2c_value;
    } as;
} HostMessage;

// Host register writes the queue had no room for. Counted in the register
// software interrupt, logged by the host thread.
static volatile uint32_t host_dropped_messages;

static bool host_state_is_live(CpuState cpu_state);
static void host_start_cpu_app(Host* instance);

// Host power. Runs on the host thread only, so the rails have one owner.

static void host_power_reset_pd_and_charger(Host* instance) {
    Pd* pd = furi_record_open(RECORD_PD);
    pd_reset_config(pd);
    furi_record_close(RECORD_PD);

    power_bq2579x_reset_config(instance->power);
}

static void host_power_on_internal(Host* instance) {
    UNUSED(instance);
    // A host still powered across an MCU reset keeps running: only hand it
    // the expander back, which the reset gave to the MCU.
    if(!furi_bsp_linux_is_load()) {
        furi_bsp_linux_reset();
    }
    furi_bsp_linux_start();
}

static void host_power_off_internal(Host* instance) {
    host_power_reset_pd_and_charger(instance);
    furi_bsp_linux_reset();
}

static void host_power_restart_internal(Host* instance) {
    host_power_off_internal(instance);
    furi_bsp_linux_start();
}

static void host_power_maskrom_internal(Host* instance) {
    host_power_off_internal(instance);
    furi_bsp_linux_maskrom(true);
    furi_event_loop_timer_start(instance->maskrom_timer, HOST_MASKROM_RELEASE_MS);
}

static void host_maskrom_timer_callback(void* context) {
    UNUSED(context);
    furi_bsp_linux_maskrom(false);
}

// Response to the host going down

// The reset mode register always shows the armed response: an invalid host
// write is put back.
static bool host_set_reset_response_internal(Host* instance, HostResetResponse response) {
    bool valid = response < HostResetResponseCount;
    if(valid) {
        instance->reset_response = response;
        FURI_LOG_I(TAG, "Host reset response: %d", instance->reset_response);
    } else {
        FURI_LOG_W(TAG, "Invalid host reset response: %u", response);
    }
    with_i2c_register({ i2c_register_update(I2C_SYSTEM_RESET_MODE_REG_ADDRESS, instance->reset_response, 0xFFFF); });
    return valid;
}

static void host_reset_state_changed(Host* instance, CpuState cpu_state) {
    if(instance->reset_response == HostResetResponseDisabled) return;

    // Only the states the kernel sends once its devices are shut down.
    // ShuttingDown comes before that, and on every kexec as well.
    if(cpu_state == CpuStatePoweredOff || cpu_state == CpuStateRestarting) {
        // Host down: (re)start the reset timer.
        uint32_t delay_ms = (uint32_t)instance->reset_delay_100ms * 100;
        FURI_LOG_I(TAG, "Host going down (state %d), arming reset timer in %lu ms", cpu_state, (unsigned long)delay_ms);
        furi_event_loop_timer_start(instance->reset_timer, delay_ms);
    } else if(
        furi_event_loop_timer_is_running(instance->reset_timer) &&
        (cpu_state == CpuStateUnknown || cpu_state == CpuStateBootloader ||
         cpu_state == CpuStateKernelInit || cpu_state == CpuStateOnline)) {
        // Host came back: cancel the pending reset
        furi_event_loop_timer_stop(instance->reset_timer);
    }
}

// Cut the host's power first, so that nothing runs on it while the MCU is
// away, then boot a staged image or reset. The next boot powers it again.
static void host_reset_timer_callback(void* context) {
    furi_assert(context);
    Host* instance = context;

    FURI_LOG_I(TAG, "Host is down, cutting its power");
    host_power_off_internal(instance);

    if(instance->reset_response == HostResetResponseResetMcuAndPower) {
        FURI_LOG_I(TAG, "Cycling system power through the charger");
        for(uint32_t attempt = 0; attempt < HOST_RESET_ATTEMPTS; attempt++) {
            if(attempt) {
                furi_delay_ms(1000);
            }
            if(power_bq2579x_set_power_switch(instance->power, Bq2579xPowerReset)) {
                break;
            }
        }
    }

    // Boot a staged image, or reset: also after the charger cycle, which only
    // handles the host, so a failing charger can't leave the MCU half-switched.
    updater_activate(instance->updater);
    FURI_LOG_I(TAG, "Resetting MCU");
    furi_hal_power_reset();
}

// SYSTEM_RESET from the host: boot a staged image, else a plain reset. The
// host keeps its power, and the next boot finds it powered.
static void host_reset_request_internal(Host* instance) {
    updater_activate(instance->updater);
    FURI_LOG_I(TAG, "Host reset request: resetting MCU");
    furi_hal_power_reset();
}

// Messages

static void host_set_cpu_state_internal(Host* instance, CpuState cpu_state) {
    instance->cpu_state = cpu_state;
    FURI_LOG_I(TAG, "CPU State: %d", instance->cpu_state);

    if(instance->cpu_state == CpuStatePoweredOff) {
        furi_event_loop_timer_start(instance->shutdown_timer, HOST_POWER_OFF_DELAY_MS);
    }

    host_reset_state_changed(instance, cpu_state);

    if(host_state_is_live(cpu_state)) {
        host_start_cpu_app(instance);
    }
}

static void host_message_queue_callback(FuriEventLoopObject* object, void* context) {
    furi_assert(context);
    Host* instance = context;
    furi_assert(object == instance->message_queue);

    HostMessage msg;
    furi_check(furi_message_queue_get(instance->message_queue, &msg, 0) == FuriStatusOk);

    bool result = true;

    switch(msg.type) {
    case HostMessageTypeSetCpuState:
        if(msg.as.cpu_state >= CpuStateNumStates) {
            // Ignore garbage from the I2C link (bus glitch, protocol/version
            // mismatch, etc.) instead of corrupting our state; log it so the
            // mismatch is still visible.
            FURI_LOG_E(TAG, "Invalid CPU state from I2C: %u", msg.as.cpu_state);
            result = false;
            break;
        }
        host_set_cpu_state_internal(instance, msg.as.cpu_state);
        break;
    case HostMessageTypePowerOn:
        host_power_on_internal(instance);
        break;
    case HostMessageTypePowerOff:
        host_power_off_internal(instance);
        break;
    case HostMessageTypePowerRestart:
        host_power_restart_internal(instance);
        break;
    case HostMessageTypePowerMaskrom:
        host_power_maskrom_internal(instance);
        break;
    case HostMessageTypeSetResetResponse:
        result = host_set_reset_response_internal(instance, msg.as.reset_response);
        break;
    case HostMessageTypeSetResetDelay:
        instance->reset_delay_100ms = msg.as.reset_delay_100ms;
        FURI_LOG_I(TAG, "Host reset delay: %u (100ms)", instance->reset_delay_100ms);
        break;
    case HostMessageTypeI2cSystemReset:
        if(msg.as.i2c_value == I2C_SYSTEM_RESET_MAGIC) {
            FURI_LOG_I(TAG, "Reset command received from host");
            host_reset_request_internal(instance);
        } else {
            FURI_LOG_W(TAG, "Invalid reset magic: 0x%04X", msg.as.i2c_value);
        }
        break;
    default:
        furi_crash("Invalid message type");
        break;
    }

    if(msg.result) {
        *msg.result = result;
    }

    if(msg.lock) {
        api_lock_unlock(msg.lock);
    }

    uint32_t dropped = host_dropped_messages;
    if(dropped != instance->dropped_messages_logged) {
        FURI_LOG_E(TAG, "%lu register writes dropped: queue full", dropped - instance->dropped_messages_logged);
        instance->dropped_messages_logged = dropped;
    }
}

static bool host_send_message(Host* instance, HostMessage* message) {
    furi_check(instance);
    bool result = false;
    message->result = &result;
    message->lock = api_lock_alloc_locked();
    furi_check(furi_message_queue_put(instance->message_queue, message, FuriWaitForever) == FuriStatusOk);
    api_lock_wait_unlock_and_free(message->lock);
    return result;
}

// CPU app: the host's screen

static void host_shutdown_timer_callback(void* context) {
    furi_check(context);
    Host* instance = context;
    if(instance->cpu_state == CpuStatePoweredOff) {
        const char* appid = desktop_get_running_app_id();

        if(appid && (!strcmp(appid, "cpu_app_start") || !strcmp(appid, "cpu_app_maskrom"))) {
            FURI_LOG_I(TAG, "Cpu_app stopping due to CPU State: %d", instance->cpu_state);
            desktop_stop_app(appid);
        }
    }
}

static bool host_state_is_live(CpuState cpu_state) {
    return cpu_state == CpuStateBootloader || cpu_state == CpuStateKernelInit ||
           cpu_state == CpuStateOnline;
}

// Start the CPU app unless the user is in another app; stops self_check and
// retries on a timer until it releases the slot.
static void host_start_cpu_app(Host* instance) {
    const char* appid = desktop_get_running_app_id();
    if(appid && (!strcmp(appid, "cpu_app_start") || !strcmp(appid, "cpu_app_maskrom"))) {
        return; // already showing the host
    }

    if(appid && strcmp(appid, "self_check") == 0) {
        FURI_LOG_I(TAG, "Host live, stopping self_check to start CPU app");
        desktop_stop_app(appid);
        // self_check may take a beat to release the desktop slot; retry.
        furi_event_loop_timer_start(instance->cpu_app_retry_timer, HOST_CPU_APP_RETRY_MS);
        return;
    }

    if(appid == NULL) {
        FURI_LOG_I(TAG, "Host live, starting CPU app");
        for(size_t i = 0; i < FLIPPER_APPS_COUNT; i++) {
            if(strcmp(FLIPPER_APPS[i].appid, "cpu_app_start") == 0) {
                // self_check autorun may register in between: try again then
                if(!desktop_start_app(&FLIPPER_APPS[i])) {
                    furi_event_loop_timer_start(instance->cpu_app_retry_timer, HOST_CPU_APP_RETRY_MS);
                }
                break;
            }
        }
        return;
    }

    FURI_LOG_D(TAG, "App %s running, leaving it alone", appid);
}

static void host_cpu_app_retry_timer_callback(void* context) {
    furi_check(context);
    host_start_cpu_app(context);
}

// Pick the boot path. An MCU reset leaves the host rails as they were, and
// Linux only ever holds them on, so a powered host is running: show it, and
// Linux replays its state on the restart interrupt. A switch to a new image
// (a flash-update boot, bought by now: the updater record comes after the
// buy) and a try-before-you-buy revert (the watchdog fired) follow a host
// that went down, whose power the old image cut: power it again.
static void host_boot_start(Host* instance) {
    // Power first: the screen may wait for self_check, the host need not.
    if(rom_get_last_boot_type() == BOOT_TYPE_FLASH_UPDATE) {
        FURI_LOG_I(TAG, "Flash-update boot: starting the host");
        host_power_on_internal(instance);
        host_start_cpu_app(instance);
    } else if(watchdog_hw->reason & WATCHDOG_REASON_TIMER_BITS) {
        FURI_LOG_I(TAG, "Watchdog reset, an update reverted: starting the host");
        host_power_on_internal(instance);
        host_start_cpu_app(instance);
    } else if(furi_bsp_linux_is_load()) {
        FURI_LOG_I(TAG, "Host is powered: starting CPU app");
        host_start_cpu_app(instance);
    }
}

// I2C registers. The write callbacks run in the register software interrupt:
// they only queue the value for the host thread, without waiting.

static void host_i2c_post(void* context, HostMessage message) {
    furi_check(context);
    Host* instance = context;
    FuriStatus status = furi_message_queue_put(instance->message_queue, &message, 0);
    furi_check(status != FuriStatusErrorParameter);
    if(status != FuriStatusOk) host_dropped_messages = host_dropped_messages + 1;
}

static void host_on_i2c_cpu_state_write(void* context, uint16_t address, uint16_t value) {
    UNUSED(address);
    host_i2c_post(context, (HostMessage){.type = HostMessageTypeSetCpuState, .as.cpu_state = (CpuState)value});
}

static void host_on_i2c_system_reset_write(void* context, uint16_t address, uint16_t value) {
    UNUSED(address);
    host_i2c_post(context, (HostMessage){.type = HostMessageTypeI2cSystemReset, .as.i2c_value = value});
}

static void host_on_i2c_reset_mode_write(void* context, uint16_t address, uint16_t value) {
    UNUSED(address);
    host_i2c_post(context, (HostMessage){.type = HostMessageTypeSetResetResponse, .as.reset_response = (HostResetResponse)value});
}

static void host_on_i2c_reset_delay_write(void* context, uint16_t address, uint16_t value) {
    UNUSED(address);
    host_i2c_post(context, (HostMessage){.type = HostMessageTypeSetResetDelay, .as.reset_delay_100ms = value});
}

void host_i2c_setup(Host* instance) {
    furi_check(instance);
    i2c_register_add_writable(I2C_CPU_STATUS_REG_ADDRESS, 0, host_on_i2c_cpu_state_write, instance);
    i2c_register_add_writable(I2C_SYSTEM_RESET_REG_ADDRESS, 0, host_on_i2c_system_reset_write, instance);
    i2c_register_add_writable(I2C_SYSTEM_RESET_MODE_REG_ADDRESS, HostResetResponseDisabled, host_on_i2c_reset_mode_write, instance);
    i2c_register_add_writable(I2C_SYSTEM_RESET_DELAY_REG_ADDRESS, 0, host_on_i2c_reset_delay_write, instance);

    // Unmasked by default: the host restores its masks only once this fires.
    i2c_register_add_interrupt(I2C_SYSTEM_INTERRUPT_REG_ADDRESS, I2C_SYSTEM_INTERRUPT_MASK_REG_ADDRESS, I2C_STATUS_REG_BIT_SYSTEM);
    // Every boot is a restart for the host: whatever it wrote before is gone.
    with_i2c_register({ i2c_register_set_interrupt(I2C_SYSTEM_INTERRUPT_REG_ADDRESS, 1 << I2C_SYSTEM_INTERRUPT_REG_BIT_RESET); });
}

static Host* host_alloc(void) {
    Host* instance = (Host*)malloc(sizeof(Host));
    instance->cpu_state = CpuStateUnknown;
    instance->reset_response = HostResetResponseDisabled;
    instance->event_loop = furi_event_loop_alloc();
    instance->message_queue = furi_message_queue_alloc(HOST_MAX_MESSAGES, sizeof(HostMessage));
    instance->shutdown_timer = furi_event_loop_timer_alloc(instance->event_loop, host_shutdown_timer_callback, FuriEventLoopTimerTypeOnce, instance);
    instance->cpu_app_retry_timer = furi_event_loop_timer_alloc(instance->event_loop, host_cpu_app_retry_timer_callback, FuriEventLoopTimerTypeOnce, instance);
    instance->maskrom_timer = furi_event_loop_timer_alloc(instance->event_loop, host_maskrom_timer_callback, FuriEventLoopTimerTypeOnce, instance);
    instance->reset_timer = furi_event_loop_timer_alloc(instance->event_loop, host_reset_timer_callback, FuriEventLoopTimerTypeOnce, instance);
    instance->power = furi_record_open(RECORD_POWER);
    instance->updater = furi_record_open(RECORD_UPDATER);

    furi_event_loop_subscribe_message_queue(instance->event_loop, instance->message_queue, FuriEventLoopEventIn, host_message_queue_callback, instance);

    furi_record_create(RECORD_HOST, instance);

    host_boot_start(instance);

    return instance;
}

int32_t host_srv(void* p) {
    UNUSED(p);

    Host* instance = host_alloc();
    furi_event_loop_run(instance->event_loop);

    return 0;
}

void host_power_on(Host* instance) {
    HostMessage msg = {.type = HostMessageTypePowerOn};
    host_send_message(instance, &msg);
}

void host_power_off(Host* instance) {
    HostMessage msg = {.type = HostMessageTypePowerOff};
    host_send_message(instance, &msg);
}

void host_power_restart(Host* instance) {
    HostMessage msg = {.type = HostMessageTypePowerRestart};
    host_send_message(instance, &msg);
}

void host_power_maskrom(Host* instance) {
    HostMessage msg = {.type = HostMessageTypePowerMaskrom};
    host_send_message(instance, &msg);
}
