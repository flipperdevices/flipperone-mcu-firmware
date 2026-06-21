#pragma once

#include <furi.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Initialize the I2C watchdog and register the kick address. */
void i2c_watchdog_init(uint32_t timeout_ms);

/** Start the watchdog timer. Expects periodic kicks via I2C after this call. */
void i2c_watchdog_start(void);

/** Stop the watchdog timer. Kicks are ignored while stopped. */
void i2c_watchdog_stop(void);

/** Update the watchdog timeout. Restarts the timer if it is already running. */
void i2c_watchdog_set_timeout(uint32_t timeout_ms);

/** PubSub notified when the watchdog fires. Message is always NULL. */
FuriPubSub* i2c_watchdog_get_pubsub(void);

#ifdef __cplusplus
}
#endif
