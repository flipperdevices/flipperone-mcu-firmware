#include "power_meter_i.h"
#include <led/led.h>

#define TAG "PowerMeter"

#define POWER_METER_UPDATE_PERIOD_MS 200

#define BLINK_PERIOD_MS 500

#define BATTERY_LOW_THRESHOLD_PERCENT 5

typedef struct {
    void* context;
    FuriEventLoop* event_loop;

    FuriEventLoopTimer* update_timer;
    FuriEventLoopTimer* blink_timer;

    LedColor in_color_last;
    LedColor battery_color_last;
    bool battery_blink;
    bool battery_charging;
    uint8_t in_power_level_last;
    uint8_t consumption_level_last;

    bool blink_state;
} PowerMeter;

static const struct {
    uint32_t threshold_mw;
    LedColor color[4]; // 4 level leds, bottom to top
} power_consumption_levels[] = {
    {0, {LED_COLOR_BLACK, LED_COLOR_BLACK, LED_COLOR_BLACK, LED_COLOR_BLACK}},
    {300, {LED_COLOR_GREEN, LED_COLOR_BLACK, LED_COLOR_BLACK, LED_COLOR_BLACK}},
    {2000, {LED_COLOR_GREEN, LED_COLOR_YELLOW, LED_COLOR_BLACK, LED_COLOR_BLACK}},
    {4000, {LED_COLOR_GREEN, LED_COLOR_YELLOW, LED_COLOR_ORANGE, LED_COLOR_BLACK}},
    {6000, {LED_COLOR_GREEN, LED_COLOR_YELLOW, LED_COLOR_ORANGE, LED_COLOR_RED}},
    {8000, {LED_COLOR_GREEN, LED_COLOR_ORANGE, LED_COLOR_ORANGE, LED_COLOR_RED}},
    {12000, {LED_COLOR_ORANGE, LED_COLOR_ORANGE, LED_COLOR_RED, LED_COLOR_RED}},
    {24000, {LED_COLOR_ORANGE, LED_COLOR_RED, LED_COLOR_RED, LED_COLOR_RED}},
};

static const uint32_t in_power_thresholds_mw[] = {1000, 7500, 15000, 27000};

static void update_consumption_leds(PowerMeter* instance, PowerMeterState* state, LedBatch* batch) {
    uint8_t consumption_level = 0;
    if(!state->error) {
        consumption_level = COUNT_OF(power_consumption_levels) - 1;
        for(size_t level = 0; level < COUNT_OF(power_consumption_levels) - 1; level++) {
            if(state->power_consumption_mw < power_consumption_levels[level + 1].threshold_mw) {
                consumption_level = level;
                break;
            }
        }
    }

    if(consumption_level != instance->consumption_level_last) {
        LedItem* leds = batch->items;
        size_t i = batch->count;
        leds[i].type = FuriBspLedTypeBatteryWatt1;
        leds[i].color = power_consumption_levels[consumption_level].color[0];
        leds[i + 1].type = FuriBspLedTypeBatteryWatt2;
        leds[i + 1].color = power_consumption_levels[consumption_level].color[1];
        leds[i + 2].type = FuriBspLedTypeBatteryWatt3;
        leds[i + 2].color = power_consumption_levels[consumption_level].color[2];
        leds[i + 3].type = FuriBspLedTypeBatteryWatt4;
        leds[i + 3].color = power_consumption_levels[consumption_level].color[3];
        batch->count += 4;
    }

    instance->consumption_level_last = consumption_level;
}

static void update_battery_leds(PowerMeter* instance, PowerMeterState* state, LedBatch* batch) {
    bool is_charging = state->power_consumption_mw < state->in_power_mw;
    uint32_t idle_threshold = power_consumption_levels[1].threshold_mw;
    bool blink = false;
    bool charge_led = false;
    LedColor color = LED_COLOR_BLACK;

    if(state->error) {
        // Error state - blink all red
        blink = true;
        charge_led = true;
        color = LED_COLOR_RED;
    } else if(!is_charging && (state->battery_percent < BATTERY_LOW_THRESHOLD_PERCENT)) {
        // Low battery - red outline
        color = LED_COLOR_RED;
    } else if(state->in_connected && !is_charging) {
        // High load - blink all yellow
        blink = true;
        charge_led = true;
        color = LED_COLOR_YELLOW;
    } else if(state->in_connected && is_charging && (state->battery_percent < 100)) {
        // Charging - all green
        charge_led = true;
        color = LED_COLOR_GREEN;
    } else if(state->in_connected && (state->battery_percent == 100)) {
        // Fully charged - all off
        color = LED_COLOR_BLACK;
    } else if(!state->in_connected && (state->power_consumption_mw < idle_threshold)) {
        // Idle - all off
        color = LED_COLOR_BLACK;
    } else {
        // Discharging - green outline
        color = LED_COLOR_GREEN;
    }

    bool need_update = (instance->battery_blink != blink) || (instance->battery_charging != charge_led) ||
                       (memcmp(&instance->battery_color_last, &color, sizeof(LedColor)) != 0);
    if(need_update) {
        LedItem* leds = batch->items;
        size_t i = batch->count;
        leds[i].type = FuriBspLedTypeBatteryOutline;
        leds[i].color = color;
        leds[i + 1].type = FuriBspLedTypeBatteryCenter;
        leds[i + 1].color = (charge_led ? color : LED_COLOR_BLACK);
        batch->count += 2;
        if(blink) {
            furi_event_loop_timer_start(instance->blink_timer, BLINK_PERIOD_MS);
            instance->blink_state = true;
        } else {
            furi_event_loop_timer_stop(instance->blink_timer);
        }
    }

    instance->battery_blink = blink;
    instance->battery_charging = charge_led;
    instance->battery_color_last = color;
}

static void power_meter_blink_timer_callback(void* context) {
    furi_check(context);
    PowerMeter* instance = context;

    if(instance->battery_blink) {
        instance->blink_state = !instance->blink_state;
        LedColor color = instance->blink_state ? instance->battery_color_last : LED_COLOR_BLACK;

        LedItem leds[2];
        size_t led_count = 0;
        leds[0].type = FuriBspLedTypeBatteryOutline;
        leds[0].color = color;
        if(instance->battery_charging) {
            leds[1].type = FuriBspLedTypeBatteryCenter;
            leds[1].color = color;
            led_count = 2;
        } else {
            led_count = 1;
        }

        Led* led = furi_record_open(RECORD_LEDS);
        LedBatch batch = {.items = leds, .count = led_count};
        led_set_color_batch(led, &batch);
        furi_record_close(RECORD_LEDS);
    }
}

static void update_input_leds(PowerMeter* instance, PowerMeterState* state, LedBatch* batch) {
    uint8_t in_power_level = 0;
    if((state->in_connected) && (!state->error)) {
        in_power_level = COUNT_OF(in_power_thresholds_mw);
        for(size_t level = 0; level < COUNT_OF(in_power_thresholds_mw); level++) {
            if(state->in_power_mw < in_power_thresholds_mw[level]) {
                in_power_level = level;
                break;
            }
        }
    }

    LedColor in_color = (state->in_connected ? (state->in_pd_valid ? LED_COLOR_GREEN : LED_COLOR_YELLOW) : LED_COLOR_BLACK);
    if(memcmp(&in_color, &instance->in_color_last, sizeof(LedColor)) != 0) {
        LedItem* leds = batch->items;
        leds[batch->count].type = FuriBspLedTypeUsbCharging;
        leds[batch->count].color = in_color;
        batch->count++;
    }
    instance->in_color_last = in_color;

    if(in_power_level != instance->in_power_level_last) {
        LedItem* leds = batch->items;
        size_t i = batch->count;
        leds[i].type = FuriBspLedTypeUsbWatt1;
        leds[i].color = (in_power_level >= 1 ? LED_COLOR_GREEN : LED_COLOR_BLACK);
        leds[i + 1].type = FuriBspLedTypeUsbWatt2;
        leds[i + 1].color = (in_power_level >= 2 ? LED_COLOR_GREEN : LED_COLOR_BLACK);
        leds[i + 2].type = FuriBspLedTypeUsbWatt3;
        leds[i + 2].color = (in_power_level >= 3 ? LED_COLOR_GREEN : LED_COLOR_BLACK);
        leds[i + 3].type = FuriBspLedTypeUsbWatt4;
        leds[i + 3].color = (in_power_level >= 4 ? LED_COLOR_GREEN : LED_COLOR_BLACK);
        batch->count += 4;
    }
    instance->in_power_level_last = in_power_level;
}

static void power_meter_update_timer_callback(void* context) {
    furi_check(context);
    PowerMeter* instance = context;

    PowerMeterState state;
    power_meter_test_get_state(&state, instance->context);

    LedItem* leds = malloc(sizeof(LedItem) * 11); // 11 leds total
    LedBatch batch = {.items = leds, .count = 0};

    update_consumption_leds(instance, &state, &batch);
    update_battery_leds(instance, &state, &batch);
    update_input_leds(instance, &state, &batch);

    if(batch.count > 0) {
        Led* led = furi_record_open(RECORD_LEDS);
        led_set_color_batch(led, &batch);
        furi_record_close(RECORD_LEDS);
    }
    free(leds);
}

int32_t power_meter_srv(void* p) {
    UNUSED(p);

    PowerMeter* instance = malloc(sizeof(PowerMeter));
    instance->context = p;
    instance->event_loop = furi_event_loop_alloc();

    instance->update_timer = furi_event_loop_timer_alloc(instance->event_loop, power_meter_update_timer_callback, FuriEventLoopTimerTypePeriodic, instance);
    instance->blink_timer = furi_event_loop_timer_alloc(instance->event_loop, power_meter_blink_timer_callback, FuriEventLoopTimerTypePeriodic, instance);

    furi_event_loop_timer_start(instance->update_timer, POWER_METER_UPDATE_PERIOD_MS);

    furi_event_loop_run(instance->event_loop);

    furi_event_loop_timer_free(instance->update_timer);
    furi_event_loop_timer_free(instance->blink_timer);
    furi_event_loop_free(instance->event_loop);
    free(instance);

    return 0;
}
