#pragma once

typedef enum {
    Bq25792PowerIdle = 0b00, /** Normal operation (default) */
    Bq25792PowerShutdown = 0b01, /** Shutdown mode*/
    Bq25792PowerShipMode = 0b10, /** Ship mode*/
    Bq25792PowerReset = 0b11, /** System power reset*/
} Bq25792PowerSwitch;


