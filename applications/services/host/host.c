#include "host.h"

#include <api_lock.h>
#include <desktop/desktop.h>
#include <power/power.h>
#include <updater/updater.h>
#include <pd/pd.h>
#include <furi_bsp_linux.h>
#include <i2c_intercom/i2c_registers.h>
#include <i2c_intercom/i2c_registers_map.h>
#include <hardware/structs/powman.h>

#define TAG "Host"

#define HOST_MAX_MESSAGES          (8)
#define HOST_POWER_OFF_DELAY_MS    (300) // after powered-off, unless a switch is armed
#define HOST_MASKROM_RELEASE_MS    (1000)
#define HOST_CPU_APP_RETRY_MS      (1000) // wait for self_check to release the desktop
// How long Linux gets to write its CPU state before the host counts as not
// responding. Its driver replays the state ~350 ms after an MCU reset; a cold
// boot first reports ~3 s in, the margin is for a slower loader.
#define HOST_RUNNING_RESPONSE_MS (5000)
#define HOST_STARTING_RESPONSE_MS (30000)

// Set before a switch when the host restarts, so the image that boots next
// (the new one, or the old one after a revert) powers it again; a host that
// powered off stays off. POWMAN scratch survives every reset but power-on.
#define HOST_RESTART_MARKER       (powman_hw->scratch[1])
#define HOST_RESTART_MARKER_MAGIC (0x4057B007)

struct Host {
    FuriEventLoop* event_loop;
    FuriMessageQueue* message_queue;
    CpuState cpu_state;
    FuriEventLoopTimer* cpu_app_retry_timer;
    FuriEventLoopTimer* maskrom_timer;
    FuriEventLoopTimer* down_timer;
    FuriEventLoopTimer* response_timer;
    HostState state;
    bool reported; // Linux wrote its CPU state since the host was powered
    FuriPubSub* pubsub;
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
    } as;
} HostMessage;

// Host register writes the queue had no room for. Counted in the register
// software interrupt, logged by the host thread.
static volatile uint32_t host_dropped_messages;

static bool host_state_is_live(CpuState cpu_state);
static void host_start_cpu_app(Host* instance);

// Host state

// The host draws only on a change: ask for its current screen
static void host_request_frame(void) {
    with_i2c_register({ i2c_register_set_interrupt(I2C_SYSTEM_INTERRUPT_REG_ADDRESS, 1 << I2C_SYSTEM_INTERRUPT_REG_BIT_FRAME); });
}

static void host_set_state(Host* instance, HostState state) {
    if(instance->state == state) return;
    const HostState was = instance->state;
    instance->state = state;
    FURI_LOG_I(TAG, "Host state: %d", state);
    furi_pubsub_publish(instance->pubsub, &state);
    // From a boot phase its frames were dropped, the first of this kernel's
    // too; from not responding it may have nothing new to draw
    if(state == HostStateRunning && (was == HostStateStarting || was == HostStateHandover || was == HostStateMaskrom || was == HostStateNotResponding)) {
        host_request_frame();
    }
}

// The host was just powered: Linux has to write its CPU state in time.
static void host_await_report(Host* instance, HostState state, uint32_t timeout_ms) {
    instance->reported = false;
    furi_event_loop_timer_start(instance->response_timer, timeout_ms);
    host_set_state(instance, state);
}

static void host_response_timer_callback(void* context) {
    furi_check(context);
    Host* instance = context;
    FURI_LOG_W(TAG, "Host has not reported its CPU state");
    host_set_state(instance, HostStateNotResponding);
}

static void host_reported(Host* instance, CpuState cpu_state) {
    if(instance->state == HostStateOff) return;
    // A kernel booted after maskrom too: flashed, then reset from the PC
    const bool booting = instance->state == HostStateStarting || instance->state == HostStateHandover || instance->state == HostStateMaskrom;
    // While booting only a live state answers: a kernel going down still reports
    if(!instance->reported && (!booting || host_state_is_live(cpu_state))) {
        instance->reported = true;
        furi_event_loop_timer_stop(instance->response_timer);
    }
    if(instance->state == HostStateNotResponding || (booting && host_state_is_live(cpu_state))) {
        host_set_state(instance, HostStateRunning);
    }
}

// Host power. Runs on the host thread only, so the rails have one owner.

static void host_power_cut(Host* instance) {
    Pd* pd = furi_record_open(RECORD_PD);
    pd_reset_config(pd);
    furi_record_close(RECORD_PD);

    power_bq2579x_reset_config(instance->power);

    furi_bsp_linux_reset();
    // What the host reported is gone with its power, so a stale state does not
    // read as part of the next boot: Unknown until it reports again
    instance->cpu_state = CpuStateUnknown;
    furi_event_loop_timer_stop(instance->response_timer);
    // A pending powered-off from before is answered by this cut: a host
    // powered again right after must not be cut by it.
    furi_event_loop_timer_stop(instance->down_timer);
    furi_event_loop_timer_stop(instance->cpu_app_retry_timer);
}

static void host_power_on_internal(Host* instance) {
    if(!furi_bsp_linux_is_load()) {
        furi_bsp_linux_reset();
        furi_bsp_linux_start();
        host_await_report(instance, HostStateStarting, HOST_STARTING_RESPONSE_MS);
        return;
    }
    // A host still powered across an MCU reset keeps running: only hand it
    // the expander back, which the reset gave to the MCU.
    furi_bsp_linux_start();
    if(instance->state == HostStateOff) {
        host_await_report(instance, HostStateRunning, HOST_RUNNING_RESPONSE_MS);
    }
}

// Both the host going down and the CPU app leaving its screen power the host
// off: the second one finds it off already.
static void host_power_off_internal(Host* instance) {
    if(instance->state == HostStateOff) return;
    host_power_cut(instance);
    host_set_state(instance, HostStateOff);
}

static void host_power_restart_internal(Host* instance) {
    host_power_cut(instance);
    furi_bsp_linux_start();
    host_await_report(instance, HostStateStarting, HOST_STARTING_RESPONSE_MS);
}

static void host_power_maskrom_internal(Host* instance) {
    host_power_cut(instance);
    furi_bsp_linux_maskrom(true);
    furi_event_loop_timer_start(instance->maskrom_timer, HOST_MASKROM_RELEASE_MS);
    host_set_state(instance, HostStateMaskrom);
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

// Cut the host's power. With a switch armed, then boot the staged image, which
// powers the host again if it was restarting. Otherwise only leave its screen.
static void host_down_timer_callback(void* context) {
    furi_assert(context);
    Host* instance = context;
    if(!host_is_down(instance, instance->cpu_state)) return; // disarmed meanwhile
    bool armed = host_switch_armed(instance);
    // The cut forgets the CPU state
    const bool restarting = instance->cpu_state == CpuStateRestarting;

    FURI_LOG_I(TAG, "Host is down, cutting its power");
    host_power_off_internal(instance);

    if(armed) {
        HOST_RESTART_MARKER = restarting ? HOST_RESTART_MARKER_MAGIC : 0;
        updater_activate(instance->updater);
        HOST_RESTART_MARKER = 0;
        // Returns only if the staged image went away meanwhile: a restarting
        // host still expects to come back.
        if(restarting) {
            FURI_LOG_E(TAG, "Switch failed, powering the host again");
            host_power_on_internal(instance);
            return;
        }
    }

    host_stop_cpu_app(instance);
}

// Messages

static void host_set_cpu_state_internal(Host* instance, CpuState cpu_state) {
    const CpuState was = instance->cpu_state;
    instance->cpu_state = cpu_state;
    FURI_LOG_I(TAG, "CPU State: %d", instance->cpu_state);

    if(cpu_state != CpuStateUnknown) host_reported(instance, cpu_state);
    host_down_state_changed(instance, cpu_state);

    // Boot phases a running host goes through by itself. Shutting down right
    // after its start, with no request, is the boot loader's kernel handing
    // over to the profile's.
    if(instance->state != HostStateOff && instance->state != HostStateMaskrom && cpu_state != was) {
        if(cpu_state == CpuStateRestarting) {
            host_await_report(instance, HostStateStarting, HOST_STARTING_RESPONSE_MS);
        } else if(cpu_state == CpuStateShuttingDown && was == CpuStateKernelInit) {
            host_await_report(instance, HostStateHandover, HOST_STARTING_RESPONSE_MS);
        }
    }

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
            // A bus glitch or a newer driver: keep the state
            FURI_LOG_E(TAG, "Invalid CPU state from I2C: %u", msg.as.cpu_state);
            break;
        }
        host_set_cpu_state_internal(instance, msg.as.cpu_state);
        break;
    case HostMessageTypePowerOn:
        host_power_on_internal(instance);
        if(instance->state == HostStateRunning || instance->state == HostStateNotResponding) {
            FURI_LOG_I(TAG, "Host already running: requesting its screen");
            host_request_frame();
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

// Leave the host's screen.
static void host_stop_cpu_app(Host* instance) {
    UNUSED(instance);
    if(host_cpu_app_running()) {
        FURI_LOG_I(TAG, "Stopping CPU app");
        desktop_stop_app(desktop_get_running_app_id());
    }
}

static bool host_state_is_live(CpuState cpu_state) {
    return cpu_state == CpuStateBootloader || cpu_state == CpuStateKernelInit || cpu_state == CpuStateOnline;
}

// Start the CPU app unless the user is in another app; stops self_check and
// retries on a timer until it releases the slot.
static void host_start_cpu_app(Host* instance) {
    // A state queued before a cut must not bring the app, and with it the power, back
    if(instance->state == HostStateOff || host_cpu_app_running()) return;
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
// that went down, whose power the old image cut: power it again if it was
// restarting.
static void host_boot_start(Host* instance) {
    bool restart = HOST_RESTART_MARKER == HOST_RESTART_MARKER_MAGIC;
    HOST_RESTART_MARKER = 0;

    // Power first: the screen may wait for self_check, the host need not.
    UpdaterBootType boot_type = updater_get_boot_type(instance->updater);
    const char* after = boot_type == UpdaterBootTypeFlashUpdate ? "Switched to a new image" : boot_type == UpdaterBootTypeReverted ? "Update reverted" : NULL;
    if(after && restart) {
        FURI_LOG_I(TAG, "%s: starting the host", after);
        host_power_on_internal(instance);
        host_start_cpu_app(instance);
    } else if(furi_bsp_linux_is_load()) {
        FURI_LOG_I(TAG, "Host is powered: starting CPU app");
        host_await_report(instance, HostStateRunning, HOST_RUNNING_RESPONSE_MS);
        host_start_cpu_app(instance);
    } else if(after) {
        FURI_LOG_I(TAG, "%s: the host powered off, leaving it off", after);
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
    instance->response_timer = furi_event_loop_timer_alloc(instance->event_loop, host_response_timer_callback, FuriEventLoopTimerTypeOnce, instance);
    instance->state = HostStateOff;
    instance->pubsub = furi_pubsub_alloc();
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

HostState host_get_state(Host* instance) {
    furi_check(instance);
    // One word written by the host thread only
    return instance->state;
}

FuriPubSub* host_get_pubsub(Host* instance) {
    furi_check(instance);
    return instance->pubsub;
}
