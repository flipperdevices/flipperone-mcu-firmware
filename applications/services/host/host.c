#include "host.h"

#include <api_lock.h>
#include <desktop/desktop.h>
#include <power/power.h>
#include <pd/pd.h>
#include <furi_bsp_linux.h>
#include <furi_hal_power.h>
#include <furi_hal_flash.h>
#include <pico/bootrom.h>
#include <boot/bootrom_constants.h>
#include <hardware/structs/powman.h>

#define TAG "Host"

#define HOST_MAX_MESSAGES          (8)
#define HOST_POWER_OFF_DELAY_MS    (300)
#define HOST_MASKROM_RELEASE_MS    (1000)
#define HOST_UPDATE_REBOOT_DELAY_MS (10000) // host power-cycle delay after a switch
#define HOST_RESET_ATTEMPTS        (3) // charger power-reset tries before giving up
// Boot probe: catch host announcements lost in an MCU reset (skipped on
// flash-update boots).
#define HOST_BOOT_PROBE_DELAY_MS   (3000)
#define HOST_BOOT_PROBE_RETRY_MS   (1000)
#define HOST_BOOT_PROBE_TIMEOUT_MS (30000)
// Host reset-request marker, rides POWMAN scratch across the SYSRESETREQ.
#define HOST_RESET_REQUEST_MAGIC (0x48525354u)

struct Host {
    FuriEventLoop* event_loop;
    FuriMessageQueue* message_queue;
    CpuState cpu_state;
    FuriEventLoopTimer* shutdown_timer;
    FuriEventLoopTimer* boot_probe_timer;
    FuriEventLoopTimer* maskrom_timer;
    FuriEventLoopTimer* reset_timer;
    FuriEventLoopTimer* update_reboot_timer;
    uint32_t boot_probe_deadline;
    bool reset_request;
    HostResetResponse reset_response;
    uint16_t reset_delay_100ms;
    size_t update_base;
    Power* power;
};

typedef enum {
    HostMessageTypeSetCpuState,
    HostMessageTypeGetCpuState,
    HostMessageTypePowerOn,
    HostMessageTypePowerOff,
    HostMessageTypePowerRestart,
    HostMessageTypePowerMaskrom,
    HostMessageTypeSetResetResponse,
    HostMessageTypeGetResetResponse,
    HostMessageTypeSetResetDelay,
    HostMessageTypeSetUpdateBase,
    HostMessageTypeResetRequest,
} HostMessageType;

typedef struct {
    HostMessageType type;
    FuriApiLock lock;
    bool* result;
    union {
        CpuState cpu_state;
        CpuState* cpu_state_out;
        HostResetResponse reset_response;
        HostResetResponse* reset_response_out;
        uint16_t reset_delay_100ms;
        size_t update_base;
    } as;
} HostMessage;

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
    if(!furi_bsp_linux_is_load()) {
        furi_bsp_linux_reset();
        furi_bsp_linux_start();
    }
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

static bool host_set_reset_response_internal(Host* instance, HostResetResponse response) {
    if(response >= HostResetResponseCount) {
        FURI_LOG_W(TAG, "Invalid host reset response: %u", response);
        return false;
    }
    instance->reset_response = response;
    FURI_LOG_I(TAG, "Host reset response: %d", instance->reset_response);
    return true;
}

static void host_reset_state_changed(Host* instance, CpuState cpu_state) {
    if(instance->reset_response == HostResetResponseDisabled) return;

    if(cpu_state == CpuStateShuttingDown || cpu_state == CpuStatePoweredOff) {
        // Host going down: (re)start the reset timer.
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

// A flash-update boot needs a valid image header in the target slot.
static bool host_update_base_is_bootable(Host* instance) {
    if(instance->update_base == 0) return false;
    FuriHalFwVersion version;
    return furi_hal_flash_get_image_version(furi_hal_flash_get_read_ptr(instance->update_base), &version);
}

static void host_reset_timer_callback(void* context) {
    furi_assert(context);
    Host* instance = context;

    switch(instance->reset_response) {
    case HostResetResponseResetMcu:
        if(host_update_base_is_bootable(instance)) {
            FURI_LOG_I(TAG, "Host is down, booting updated partition at 0x%08lX", (unsigned long)instance->update_base);
            furi_hal_power_reboot_to_partition((uint32_t)instance->update_base);
        } else {
            FURI_LOG_I(TAG, "Host is down, resetting MCU");
            furi_hal_power_reset();
        }
        break;
    case HostResetResponseResetMcuAndPower:
        FURI_LOG_I(TAG, "Host is down, resetting MCU and cycling host power");
        host_power_reset_pd_and_charger(instance);
        for(uint32_t attempt = 0; attempt < HOST_RESET_ATTEMPTS; attempt++) {
            if(attempt) {
                furi_delay_ms(1000);
            }
            if(power_bq2579x_set_power_switch(instance->power, Bq2579xPowerReset)) {
                break;
            }
        }
        // Reset the MCU explicitly as well: the charger cycle only handles
        // the host, and a failing charger must not leave the MCU half-switched.
        furi_hal_power_reset();
        break;
    default:
        FURI_LOG_E(TAG, "Host reset timer fired while disabled");
        break;
    }
}

// SYSTEM_RESET from the host: activate an armed image, else a marked reset.
static void host_reset_request_internal(Host* instance) {
    if(host_update_base_is_bootable(instance)) {
        FURI_LOG_I(TAG, "Host reset request: booting updated partition at 0x%08lX", (unsigned long)instance->update_base);
        furi_hal_power_reboot_to_partition((uint32_t)instance->update_base);
    } else {
        FURI_LOG_I(TAG, "Host reset request: resetting MCU");
        powman_hw->boot[0] = HOST_RESET_REQUEST_MAGIC;
        furi_hal_power_reset();
    }
}

/* A flash-update boot strands the host mid-reboot: cycle its rails (PD +
 * charger + expander) so it comes back. */
static void host_update_reboot_timer_callback(void* context) {
    furi_assert(context);
    Host* instance = context;

    FURI_LOG_I(TAG, "Flash-update boot: power-cycling the host");
    host_power_restart_internal(instance);
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
    case HostMessageTypeGetCpuState:
        *msg.as.cpu_state_out = instance->cpu_state;
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
    case HostMessageTypeGetResetResponse:
        *msg.as.reset_response_out = instance->reset_response;
        break;
    case HostMessageTypeSetResetDelay:
        instance->reset_delay_100ms = msg.as.reset_delay_100ms;
        FURI_LOG_I(TAG, "Host reset delay: %u (100ms)", instance->reset_delay_100ms);
        break;
    case HostMessageTypeSetUpdateBase:
        instance->update_base = msg.as.update_base;
        FURI_LOG_I(TAG, "Host reset update base: 0x%08lX", (unsigned long)instance->update_base);
        break;
    case HostMessageTypeResetRequest:
        host_reset_request_internal(instance);
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
// retries via the probe timer until it releases the slot.
static void host_start_cpu_app(Host* instance) {
    const char* appid = desktop_get_running_app_id();
    if(appid && (!strcmp(appid, "cpu_app_start") || !strcmp(appid, "cpu_app_maskrom"))) {
        return; // already showing the host
    }

    if(appid && strcmp(appid, "self_check") == 0) {
        FURI_LOG_I(TAG, "Host live, stopping self_check to start CPU app");
        desktop_stop_app(appid);
        // self_check may take a beat to release the desktop slot; retry.
        furi_event_loop_timer_start(instance->boot_probe_timer, HOST_BOOT_PROBE_RETRY_MS);
        return;
    }

    if(appid == NULL) {
        FURI_LOG_I(TAG, "Host live, starting CPU app");
        for(size_t i = 0; i < FLIPPER_APPS_COUNT; i++) {
            if(strcmp(FLIPPER_APPS[i].appid, "cpu_app_start") == 0) {
                desktop_start_app(&FLIPPER_APPS[i]);
                break;
            }
        }
        return;
    }

    FURI_LOG_D(TAG, "App %s running, leaving it alone", appid);
}

// Poll the desktop until the host announces or the window closes; live
// transitions afterwards are event-driven.
static void host_boot_probe_timer_callback(void* context) {
    furi_check(context);
    Host* instance = context;

    if(instance->reset_request || host_state_is_live(instance->cpu_state)) {
        host_start_cpu_app(instance);
        return;
    }

    if(furi_get_tick() < instance->boot_probe_deadline) {
        furi_event_loop_timer_start(instance->boot_probe_timer, HOST_BOOT_PROBE_RETRY_MS);
    } else {
        FURI_LOG_I(TAG, "Boot probe: host never announced (state %d), keeping default app", instance->cpu_state);
    }
}

// Pick the boot path: host-requested resets start the CPU app right away;
// flash-update boots power-cycle the host and rely on event-driven
// announcements; other normal boots probe for a late one.
static void host_boot_start(Host* instance) {
    if(instance->reset_request) {
        FURI_LOG_I(TAG, "Host reset request: starting CPU app");
        host_start_cpu_app(instance);
    } else if(rom_get_last_boot_type() != BOOT_TYPE_FLASH_UPDATE) {
        furi_event_loop_timer_start(instance->boot_probe_timer, HOST_BOOT_PROBE_DELAY_MS);
    } else {
        // After a partition switch the host is stranded mid-reboot: power-cycle it.
        FURI_LOG_I(TAG, "Flash-update boot detected, cycling host power in %lu ms", (unsigned long)HOST_UPDATE_REBOOT_DELAY_MS);
        furi_event_loop_timer_start(instance->update_reboot_timer, HOST_UPDATE_REBOOT_DELAY_MS);
    }
}

static Host* host_alloc(void) {
    Host* instance = (Host*)malloc(sizeof(Host));
    instance->cpu_state = CpuStateUnknown;
    instance->reset_response = HostResetResponseDisabled;
    instance->reset_request = (powman_hw->boot[0] == HOST_RESET_REQUEST_MAGIC);
    powman_hw->boot[0] = 0;
    instance->event_loop = furi_event_loop_alloc();
    instance->message_queue = furi_message_queue_alloc(HOST_MAX_MESSAGES, sizeof(HostMessage));
    instance->shutdown_timer = furi_event_loop_timer_alloc(instance->event_loop, host_shutdown_timer_callback, FuriEventLoopTimerTypeOnce, instance);
    instance->boot_probe_timer = furi_event_loop_timer_alloc(instance->event_loop, host_boot_probe_timer_callback, FuriEventLoopTimerTypeOnce, instance);
    instance->maskrom_timer = furi_event_loop_timer_alloc(instance->event_loop, host_maskrom_timer_callback, FuriEventLoopTimerTypeOnce, instance);
    instance->reset_timer = furi_event_loop_timer_alloc(instance->event_loop, host_reset_timer_callback, FuriEventLoopTimerTypeOnce, instance);
    instance->update_reboot_timer =
        furi_event_loop_timer_alloc(instance->event_loop, host_update_reboot_timer_callback, FuriEventLoopTimerTypeOnce, instance);
    instance->boot_probe_deadline = furi_get_tick() + HOST_BOOT_PROBE_TIMEOUT_MS;
    instance->power = furi_record_open(RECORD_POWER);

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

bool host_set_cpu_state(Host* instance, CpuState cpu_state) {
    HostMessage msg = {.type = HostMessageTypeSetCpuState, .as.cpu_state = cpu_state};
    return host_send_message(instance, &msg);
}

bool host_get_cpu_state(Host* instance, CpuState* cpu_state) {
    furi_check(cpu_state);
    HostMessage msg = {.type = HostMessageTypeGetCpuState, .as.cpu_state_out = cpu_state};
    return host_send_message(instance, &msg);
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

bool host_set_reset_response(Host* instance, HostResetResponse response) {
    HostMessage msg = {.type = HostMessageTypeSetResetResponse, .as.reset_response = response};
    return host_send_message(instance, &msg);
}

bool host_get_reset_response(Host* instance, HostResetResponse* response) {
    furi_check(response);
    HostMessage msg = {.type = HostMessageTypeGetResetResponse, .as.reset_response_out = response};
    return host_send_message(instance, &msg);
}

bool host_set_reset_delay(Host* instance, uint16_t reset_delay_100ms) {
    HostMessage msg = {.type = HostMessageTypeSetResetDelay, .as.reset_delay_100ms = reset_delay_100ms};
    return host_send_message(instance, &msg);
}

bool host_set_update_base(Host* instance, size_t update_base) {
    HostMessage msg = {.type = HostMessageTypeSetUpdateBase, .as.update_base = update_base};
    return host_send_message(instance, &msg);
}

void host_reset_request(Host* instance) {
    HostMessage msg = {.type = HostMessageTypeResetRequest};
    host_send_message(instance, &msg);
}
