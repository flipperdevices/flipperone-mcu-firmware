#include <math.h>
#include "bq25792.h"

// calculate NTC temperature from resistance
static float bq25792_ntc_temperature_from_resistance(float resistance) {
    const float beta = 3380.f;
    const float R0 = 10000.f;
    const float kelvin = 273.15f; // 0 degrees Celsius
    const float T0 = 25.f + kelvin; // 25 degrees Celsius in Kelvin

    const float T = 1.f / ((1.f / T0) + (1.f / beta) * logf(resistance / R0));
    return T - kelvin; // Convert Kelvin to Celsius
}

// calculate NTC resistance from charger adc voltage percent
static float bq25792_ntc_resistance_from_percent(float percent) {
    const float R1 = 5230.f; // 5.23kOhm
    const float R2 = 31600.f; // 31.6kOhm

    float ntc = 1.f / ((percent / (R1 * (1.f - percent))) - 1.f / R2);

    return ntc;
}

Bq25792Status bq25792_get_temperature_battery_celsius(Bq25792* instance, float* bat_temperature) {
    furi_check(instance);
    furi_check(bat_temperature);
    float temp_bat_pct;
    Bq25792Status status = bq25792_get_bat_pct(instance, &temp_bat_pct);
    if(status == Bq25792StatusOk) {
        float percent = temp_bat_pct / 100.f;
        percent = 1.f - percent;
        float ntc_resistance = bq25792_ntc_resistance_from_percent(percent);
        *bat_temperature = bq25792_ntc_temperature_from_resistance(ntc_resistance);
    }

    return status;
}
