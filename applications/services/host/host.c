#include "host.h"

#include <api_lock.h>
#include <desktop/desktop.h>
#include <power/power.h>
#include <updater/updater.h>
#include <pd/pd.h>
#include <furi_bsp_linux.h>
#include <i2c_intercom/i2c_registers.h>
#include <i2c_intercom/i2c_registers_map.h>

#define TAG "Host"

#define HOST_MAX_MESSAGES          (8)
#define HOST_POWER_OFF_DELAY_MS    (300) // after powered-off, unless a switch is armed
#define HOST_MASKROM_RELEASE_MS    (1000)
#define HOST_CPU_APP_RETRY_MS      (1000) // wait for self_check to release the desktop

struct Host {
    FuriEventLoop* event_loop;
    FuriMessageQueue* message_queue;
    CpuState cpu_state;
    FuriEventLoopTimer* cpu_app_retry_timer;
    FuriEventLoopTimer* maskrom_timer;
    FuriEventLoopTimer* down_timer;
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
} HostMessageType;

typedef struct {
    HostMessageType type;
    FuriApiLock lock;
    union {
        CpuState cpu_state;
        HostResetResponse reset_response;
        uint16_t reset_delay_100ms;
        bool* was_powered_out;
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

// Returns whether the host was already powered.
static bool host_power_on_internal(Host* instance) {
    UNUSED(instance);
    // A host still powered across an MCU reset keeps running: only hand it
    // the expander back, which the reset gave to the MCU.
    bool was_powered = furi_bsp_linux_is_load();
    if(!was_powered) {
        furi_bsp_linux_reset();
    }
    furi_bsp_linux_start();
    return was_powered;
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

// The host going down

static void host_stop_cpu_app(Host* instance);

// The reset mode register always shows the armed response: an invalid host
// write is put back.
static void host_set_reset_response_internal(Host* instance, HostResetResponse response) {
    if(response < HostResetResponseCount) {
        instance->reset_response = response;
        FURI_LOG_I(TAG, "Host reset response: %d", instance->reset_response);
    } else {
        FURI_LOG_W(TAG, "Invalid host reset response: %u", response);
    }
    with_i2c_register({ i2c_register_update(I2C_SYSTEM_RESET_MODE_REG_ADDRESS, instance->reset_response, 0xFFFF); });
}

// Armed by the host, and there is an image to switch to: the mode outlives a
// later update that fails or is aborted.
static bool host_switch_armed(Host* instance) {
    return instance->reset_response == HostResetResponseResetMcu && updater_has_staged_image(instance->updater);
}

// The host is down at the states the kernel sends once its devices are shut
// down: powered-off, and restarting when a switch is armed (the host restarts
// on its own otherwise). ShuttingDown comes before that, and on every kexec.
static bool host_is_down(Host* instance, CpuState cpu_state) {
    return cpu_state == CpuStatePoweredOff || (cpu_state == CpuStateRestarting && host_switch_armed(instance));
}

static void host_down_state_changed(Host* instance, CpuState cpu_state) {
    if(host_is_down(instance, cpu_state)) {
        uint32_t delay_ms = host_switch_armed(instance) ? (uint32_t)instance->reset_delay_100ms * 100 :
                                                          HOST_POWER_OFF_DELAY_MS;
        FURI_LOG_I(TAG, "Host is going down (state %d), in %lu ms", cpu_state, (unsigned long)delay_ms);
        furi_event_loop_timer_start(instance->down_timer, delay_ms);
    } else if(furi_event_loop_timer_is_running(instance->down_timer) && host_state_is_live(cpu_state)) {
        // Host came back: cancel
        furi_event_loop_timer_stop(instance->down_timer);
    }
}

// Cut the host's power. With a switch armed, then boot the staged image; the
// new image powers the host again. Otherwise only leave its screen.
static void host_down_timer_callback(void* context) {
    furi_assert(context);
    Host* instance = context;
    if(!host_is_down(instance, instance->cpu_state)) return; // disarmed meanwhile
    bool armed = host_switch_armed(instance);

    FURI_LOG_I(TAG, "Host is down, cutting its power");
    host_power_off_internal(instance);

    if(armed) {
        updater_activate(instance->updater);
        // Returns only if the staged image went away meanwhile: a restarting
        // host still expects to come back.
        if(instance->cpu_state == CpuStateRestarting) {
            FURI_LOG_E(TAG, "Switch failed, powering the host again");
            host_power_on_internal(instance);
            return;
        }
    }

    host_stop_cpu_app(instance);
}

// Messages

static void host_set_cpu_state_internal(Host* instance, CpuState cpu_state) {
    instance->cpu_state = cpu_state;
    FURI_LOG_I(TAG, "CPU State: %d", instance->cpu_state);

    host_down_state_changed(instance, cpu_state);

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

    switch(msg.type) {
    case HostMessageTypeSetCpuState:
        if(msg.as.cpu_state >= CpuStateNumStates) {
            // Ignore garbage from the I2C link (bus glitch, protocol/version
            // mismatch, etc.) instead of corrupting our state; log it so the
            // mismatch is still visible.
            FURI_LOG_E(TAG, "Invalid CPU state from I2C: %u", msg.as.cpu_state);
            break;
        }
        host_set_cpu_state_internal(instance, msg.as.cpu_state);
        break;
    case HostMessageTypePowerOn:
        *msg.as.was_powered_out = host_power_on_internal(instance);
        if(*msg.as.was_powered_out) {
            // The host draws only on a change: ask for its current screen.
            FURI_LOG_I(TAG, "Host already running: requesting its screen");
            with_i2c_register({ i2c_register_set_interrupt(I2C_SYSTEM_INTERRUPT_REG_ADDRESS, 1 << I2C_SYSTEM_INTERRUPT_REG_BIT_FRAME); });
        }
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
        host_set_reset_response_internal(instance, msg.as.reset_response);
        break;
    case HostMessageTypeSetResetDelay:
        instance->reset_delay_100ms = msg.as.reset_delay_100ms;
        FURI_LOG_I(TAG, "Host reset delay: %u (100ms)", instance->reset_delay_100ms);
        break;
    default:
        furi_crash("Invalid message type");
        break;
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

static void host_send_message(Host* instance, HostMessage* message) {
    furi_check(instance);
    message->lock = api_lock_alloc_locked();
    furi_check(furi_message_queue_put(instance->message_queue, message, FuriWaitForever) == FuriStatusOk);
    api_lock_wait_unlock_and_free(message->lock);
}

// CPU app: the host's screen

static bool host_cpu_app_running(void) {
    const char* appid = desktop_get_running_app_id();
    return appid && (!strcmp(appid, "cpu_app_start") || !strcmp(appid, "cpu_app_maskrom"));
}

// Leave the host's screen; the CPU app powers the host off on exit as well.
static void host_stop_cpu_app(Host* instance) {
    UNUSED(instance);
    if(host_cpu_app_running()) {
        FURI_LOG_I(TAG, "Stopping CPU app");
        desktop_stop_app(desktop_get_running_app_id());
    }
}

static bool host_state_is_live(CpuState cpu_state) {
    return cpu_state == CpuStateBootloader || cpu_state == CpuStateKernelInit ||
           cpu_state == CpuStateOnline;
}

// Start the CPU app unless the user is in another app; stops self_check and
// retries on a timer until it releases the slot.
static void host_start_cpu_app(Host* instance) {
    if(host_cpu_app_running()) return; // already showing the host
    const char* appid = desktop_get_running_app_id();

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
    UpdaterBootType boot_type = updater_get_boot_type(instance->updater);
    if(boot_type != UpdaterBootTypeNormal) {
        FURI_LOG_I(TAG, "%s: starting the host", boot_type == UpdaterBootTypeFlashUpdate ? "Switched to a new image" : "Update reverted");
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

static void host_on_i2c_reset_mode_write(void* context, uint16_t address, uint16_t value) {
    UNUSED(address);
    host_i2c_post(context, (HostMessage){.type = HostMessageTypeSetResetResponse, .as.reset_response = (HostResetResponse)value});
}

static void host_on_i2c_reset_delay_write(void* context, uint16_t address, uint16_t value) {
    UNUSED(address);
    host_i2c_post(context, (HostMessage){.type = HostMessageTypeSetResetDelay, .as.reset_delay_100ms = value});
}

// CPU state (0x0040), system reset mode and delay (0x0092, 0x0094) and the
// system interrupt (0x0104/0x0184).
static void host_i2c_setup(Host* instance) {
    i2c_registers_owner_begin(RECORD_HOST);
    i2c_register_add_writable(I2C_CPU_STATUS_REG_ADDRESS, 0, host_on_i2c_cpu_state_write, instance);
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
    instance->cpu_app_retry_timer = furi_event_loop_timer_alloc(instance->event_loop, host_cpu_app_retry_timer_callback, FuriEventLoopTimerTypeOnce, instance);
    instance->maskrom_timer = furi_event_loop_timer_alloc(instance->event_loop, host_maskrom_timer_callback, FuriEventLoopTimerTypeOnce, instance);
    instance->down_timer = furi_event_loop_timer_alloc(instance->event_loop, host_down_timer_callback, FuriEventLoopTimerTypeOnce, instance);
    instance->power = furi_record_open(RECORD_POWER);
    instance->updater = furi_record_open(RECORD_UPDATER);

    furi_event_loop_subscribe_message_queue(instance->event_loop, instance->message_queue, FuriEventLoopEventIn, host_message_queue_callback, instance);

    // Before the record: the slave starts once it exists, and the host API
    // raises the system interrupt.
    host_i2c_setup(instance);
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

bool host_power_on(Host* instance) {
    bool was_powered = false;
    HostMessage msg = {.type = HostMessageTypePowerOn, .as.was_powered_out = &was_powered};
    host_send_message(instance, &msg);
    return was_powered;
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
