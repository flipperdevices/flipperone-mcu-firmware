#include "cpu_mode.h"

#include <api_lock.h>
#include <desktop/desktop.h>
#include <power/power.h>

#define TAG "CpuMode"

#define CPU_MODE_MAX_MESSAGES          (8)
#define CPU_MODE_POWER_OFF_DELAY_MS    (300)
// Boot probe: show the CPU app if the host turns out live shortly after MCU
// boot. Retries cover late announcements and self_check holding the slot.
#define CPU_MODE_BOOT_PROBE_DELAY_MS   (3000)
#define CPU_MODE_BOOT_PROBE_RETRY_MS   (1000)
#define CPU_MODE_BOOT_PROBE_TIMEOUT_MS (30000)

typedef struct {
    CpuState cpu_state;
} CpuModeStatus;

struct CpuMode {
    FuriEventLoop* event_loop;
    FuriMessageQueue* message_queue;
    CpuModeStatus status;
    FuriEventLoopTimer* shutdown_timer;
    FuriEventLoopTimer* boot_probe_timer;
    uint32_t boot_probe_deadline;
    Power* power;
};

typedef enum {
    CpuModeMessageTypeSetCpuState,
    CpuModeMessageTypeGetCpuState,
} CpuModeMessageType;

typedef struct {
    CpuModeMessageType type;
    FuriApiLock lock;
    bool* result;
    union {
        CpuState cpu_state; // SetCpuState: value to apply
        CpuState* cpu_state_out; // GetCpuState: where to write the current value
    } as;
} CpuModeMessage;

static bool cpu_mode_state_is_live(CpuState cpu_state);
static void cpu_mode_start_cpu_app(CpuMode* instance);

static void cpu_mode_message_queue_callback(FuriEventLoopObject* object, void* context) {
    furi_assert(context);
    CpuMode* instance = context;
    furi_assert(object == instance->message_queue);

    CpuModeMessage msg;
    furi_check(furi_message_queue_get(instance->message_queue, &msg, 0) == FuriStatusOk);

    bool result = false;

    switch(msg.type) {
    case CpuModeMessageTypeSetCpuState: {
        CpuState cpu_state = msg.as.cpu_state;
        if(cpu_state >= CpuStateNumStates) {
            // Ignore garbage from the I2C link (bus glitch, protocol/version
            // mismatch, etc.) instead of corrupting our state; log it so the
            // mismatch is still visible.
            FURI_LOG_E(TAG, "Invalid CPU state from I2C: %u", cpu_state);
            break;
        }

        instance->status.cpu_state = cpu_state;
        FURI_LOG_I(TAG, "CPU State: %d", instance->status.cpu_state);

        if(instance->status.cpu_state == CpuStatePoweredOff) {
            furi_event_loop_timer_start(instance->shutdown_timer, CPU_MODE_POWER_OFF_DELAY_MS);
        }

        // The power service owns the armed reset-on-host-shutdown response
        power_on_host_state_changed(instance->power, cpu_state);

        // Follow host liveness: start the CPU app on live states unless the
        // user is already in another app.
        if(cpu_mode_state_is_live(cpu_state)) {
            cpu_mode_start_cpu_app(instance);
        }

        result = true;
        break;
    }
    case CpuModeMessageTypeGetCpuState:
        if(msg.as.cpu_state_out) {
            *msg.as.cpu_state_out = instance->status.cpu_state;
            result = true;
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
}

static void cpu_mode_send_message(CpuMode* instance, const CpuModeMessage* message) {
    furi_check(furi_message_queue_put(instance->message_queue, message, FuriWaitForever) == FuriStatusOk);

    if(message->lock) {
        api_lock_wait_unlock_and_free(message->lock);
    }
}

static void cpu_mode_shutdown_timer_callback(void* context) {
    furi_check(context);
    CpuMode* instance = context;
    if(instance->status.cpu_state == CpuStatePoweredOff) {
        const char* appid = desktop_get_running_app_id();

        if(appid && (!strcmp(appid, "cpu_app_start") || !strcmp(appid, "cpu_app_maskrom"))) {
            FURI_LOG_I(TAG, "Cpu_app stopping due to CPU State: %d", instance->status.cpu_state);
            desktop_stop_app(appid);
        }
    }
}

static bool cpu_mode_state_is_live(CpuState cpu_state) {
    return cpu_state == CpuStateBootloader || cpu_state == CpuStateKernelInit ||
           cpu_state == CpuStateOnline;
}

/* Start the CPU app unless the user is in another app. Stops self_check (the
 * autorun app) if it still holds the desktop slot and retries via the probe
 * timer until the slot frees up. */
static void cpu_mode_start_cpu_app(CpuMode* instance) {
    const char* appid = desktop_get_running_app_id();
    if(appid && (!strcmp(appid, "cpu_app_start") || !strcmp(appid, "cpu_app_maskrom"))) {
        return; // already showing the host
    }

    if(appid && strcmp(appid, "self_check") == 0) {
        FURI_LOG_I(TAG, "Host live, stopping self_check to start CPU app");
        desktop_stop_app(appid);
        // self_check may take a beat to release the desktop slot; retry.
        furi_event_loop_timer_start(instance->boot_probe_timer, CPU_MODE_BOOT_PROBE_RETRY_MS);
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

/* Host already live at MCU boot (our reset glitches the I2C bus, so its state
 * may arrive late): switch to the CPU app. Give up when the window closes;
 * live transitions afterwards are handled event-driven. */
static void cpu_mode_boot_probe_timer_callback(void* context) {
    furi_check(context);
    CpuMode* instance = context;

    if(cpu_mode_state_is_live(instance->status.cpu_state)) {
        cpu_mode_start_cpu_app(instance);
        return;
    }

    if(furi_get_tick() < instance->boot_probe_deadline) {
        furi_event_loop_timer_start(instance->boot_probe_timer, CPU_MODE_BOOT_PROBE_RETRY_MS);
    } else {
        FURI_LOG_I(TAG, "Boot probe: host never announced (state %d), keeping default app", instance->status.cpu_state);
    }
}

static CpuMode* cpu_mode_alloc(void) {
    CpuMode* instance = (CpuMode*)malloc(sizeof(CpuMode));
    instance->status.cpu_state = CpuStateUnknown;
    instance->event_loop = furi_event_loop_alloc();
    instance->message_queue = furi_message_queue_alloc(CPU_MODE_MAX_MESSAGES, sizeof(CpuModeMessage));
    instance->shutdown_timer = furi_event_loop_timer_alloc(instance->event_loop, cpu_mode_shutdown_timer_callback, FuriEventLoopTimerTypeOnce, instance);
    instance->boot_probe_timer = furi_event_loop_timer_alloc(instance->event_loop, cpu_mode_boot_probe_timer_callback, FuriEventLoopTimerTypeOnce, instance);
    instance->boot_probe_deadline = furi_get_tick() + CPU_MODE_BOOT_PROBE_TIMEOUT_MS;

    // The power service starts before us and hosts the host-reset response flag
    instance->power = furi_record_open(RECORD_POWER);

    furi_event_loop_subscribe_message_queue(instance->event_loop, instance->message_queue, FuriEventLoopEventIn, cpu_mode_message_queue_callback, instance);

    furi_record_create(RECORD_CPU_MODE, instance);

    furi_event_loop_timer_start(instance->boot_probe_timer, CPU_MODE_BOOT_PROBE_DELAY_MS);

    return instance;
}

int32_t cpu_mode_srv(void* p) {
    UNUSED(p);

    CpuMode* instance = cpu_mode_alloc();
    furi_event_loop_run(instance->event_loop);

    return 0;
}

bool cpu_mode_set_cpu_state(CpuMode* instance, CpuState cpu_state) {
    furi_check(instance);
    bool result = false;
    const CpuModeMessage msg = {
        .type = CpuModeMessageTypeSetCpuState,
        .result = &result,
        .lock = api_lock_alloc_locked(),
        .as = {.cpu_state = cpu_state},
    };

    cpu_mode_send_message(instance, &msg);
    return result;
}

bool cpu_mode_get_cpu_state(CpuMode* instance, CpuState* cpu_state) {
    furi_check(instance);
    bool result = false;
    const CpuModeMessage msg = {
        .type = CpuModeMessageTypeGetCpuState,
        .result = &result,
        .lock = api_lock_alloc_locked(),
        .as = {.cpu_state_out = cpu_state},
    };

    cpu_mode_send_message(instance, &msg);
    return result;
}
