#include <furi.h>

#include "i2c_watchdog.h"
#include "i2c_registers.h"
#include "i2c_registers_map.h"

typedef struct {
    FuriTimer* timer;
    FuriPubSub* pubsub;
    uint32_t timeout_ms;
    bool running;
} I2CWatchdog;

static I2CWatchdog watchdog;

static uint32_t i2c_watchdog_ms_to_ticks(uint32_t ms) {
    return (ms * furi_kernel_get_tick_frequency()) / 1000;
}

static void i2c_watchdog_timer_callback(void* context) {
    UNUSED(context);
    furi_pubsub_publish(watchdog.pubsub, NULL);
}

static void i2c_watchdog_kick_restart(void* context, uint32_t arg) {
    UNUSED(context);
    UNUSED(arg);

    if(watchdog.running) {
        furi_timer_restart(watchdog.timer, i2c_watchdog_ms_to_ticks(watchdog.timeout_ms));
    }
}

static void i2c_watchdog_kick_callback(void* context, uint16_t address, uint16_t value) {
    UNUSED(context);
    UNUSED(address);
    UNUSED(value);

    furi_timer_pending_callback(i2c_watchdog_kick_restart, NULL, 0);
}

void i2c_watchdog_init(uint32_t timeout_ms) {
    furi_check(watchdog.pubsub == NULL);

    watchdog.timeout_ms = timeout_ms;
    watchdog.pubsub = furi_pubsub_alloc();
    watchdog.timer = furi_timer_alloc(i2c_watchdog_timer_callback, FuriTimerTypeOnce, NULL);

    i2c_register_add_kickable(
        I2C_WATCHDOG_KICK_REG_ADDRESS & 0xFFFE, i2c_watchdog_kick_callback, NULL);
}

FuriPubSub* i2c_watchdog_get_pubsub(void) {
    furi_check(watchdog.pubsub);
    return watchdog.pubsub;
}

void i2c_watchdog_start(void) {
    furi_check(watchdog.timer);

    watchdog.running = true;
    furi_timer_start(watchdog.timer, i2c_watchdog_ms_to_ticks(watchdog.timeout_ms));
}

void i2c_watchdog_stop(void) {
    furi_check(watchdog.timer);

    watchdog.running = false;
    furi_timer_stop(watchdog.timer);
}

void i2c_watchdog_set_timeout(uint32_t timeout_ms) {
    furi_check(watchdog.timer);

    watchdog.timeout_ms = timeout_ms;

    if(watchdog.running) {
        furi_timer_restart(watchdog.timer, i2c_watchdog_ms_to_ticks(timeout_ms));
    }
}
