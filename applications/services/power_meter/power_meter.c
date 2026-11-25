/*
 * Power meter: the battery and USB LED groups as a live readout. Ported from
 * the power_meter_srv_test branch (where the state came from a test menu) and
 * fed from the hardware here, through the same reading the header uses
 * (power_read_charge_info): the charger's input and charge state, the gauge's
 * percent and the battery's own discharge power for the consumption bars.
 * "PD" has no API yet, so an input above 5.5 V counts as a negotiated one.
 */
#include "power_meter.h"
#include <led/led.h>
#include <power/power.h>
#include <string.h>

#define TAG "PowerMeter"

/* Once a second, the whole charge picture every tick (battery current and
 * voltage by the charger's ADC, the same figure the Linux screen shows, plus
 * the input and the charger state): with a divider above 1 the bars and the
 * charging LEDs would go out of step at every plug-in. The bus is shared with the Linux host, so
 * this stays light. */
#define POWER_METER_UPDATE_PERIOD_MS 1000
#define POWER_METER_FULL_READ_DIVIDER 1
/* Under a running Linux host: whether to read the bus at all (0: never), and
 * if so the full picture once in this many ticks (5 s). */
#define POWER_METER_SHARED_BUS_READS 0
#define POWER_METER_SHARED_BUS_DIVIDER 5
/* Full reads on every tick for this long after the battery current flips. */
#define POWER_METER_SETTLE_TICKS 4

#define BLINK_PERIOD_MS 500

#define BATTERY_LOW_THRESHOLD_PERCENT 5

/* An input above this is more than plain 5 V USB: a negotiated contract. */
#define POWER_METER_PD_VBUS_MV 5500

typedef struct {
    bool in_connected;
    bool in_pd_valid;
    bool error;
    /* The charger's own word: it is charging / it has terminated the charge.
     * The gauge rounds to 100 % well before the charger is done, so the
     * percent is not what decides "charging" or "full". */
    bool charger_charging;
    bool charge_done;
    uint8_t battery_percent;
    uint32_t power_consumption_mw;
    uint32_t in_power_mw;
} PowerMeterState;

typedef struct {
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
    uint32_t tick;
    /* The last full charge picture, refreshed every POWER_METER_FULL_READ_DIVIDER ticks. */
    PowerChargeInfo charge;
    bool charge_valid;
    /* Ticks left of full reads after the battery current changed direction:
     * the charger settles its status over a couple of seconds after a cable
     * comes or goes, so one read right at the flip catches a transient. */
    uint8_t settle_ticks;
} PowerMeter;

/* Battery discharge steps, one LED each: from 0.3 W, 1.5 W, 3 W and 5 W; the
 * top step covers everything from 5 W up. */
static const struct {
    uint32_t threshold_mw;
    LedColor color[4]; // 4 level leds, bottom to top
} power_consumption_levels[] = {
    {0, {LED_COLOR_BLACK, LED_COLOR_BLACK, LED_COLOR_BLACK, LED_COLOR_BLACK}},
    {300, {LED_COLOR_GREEN, LED_COLOR_BLACK, LED_COLOR_BLACK, LED_COLOR_BLACK}},
    {1500, {LED_COLOR_GREEN, LED_COLOR_YELLOW, LED_COLOR_BLACK, LED_COLOR_BLACK}},
    {3000, {LED_COLOR_GREEN, LED_COLOR_YELLOW, LED_COLOR_ORANGE, LED_COLOR_BLACK}},
    {5000, {LED_COLOR_GREEN, LED_COLOR_YELLOW, LED_COLOR_ORANGE, LED_COLOR_RED}},
};

static const uint32_t in_power_thresholds_mw[] = {1000, 7500, 15000, 27000};

static bool power_meter_read_state(PowerMeter* instance, PowerMeterState* state) {
    Power* power = furi_record_open(RECORD_POWER);
    /* The Linux host drives the same bus and writes to the charger (it kicks
     * the charger's watchdog); a read of ours colliding with such a write
     * spoils it, and enough spoiled kicks expire the watchdog, which resets
     * the charger. So under a running host the bus is touched as little as
     * the bars allow: the cable status and the battery's current and voltage
     * every tick (three short transactions), the whole charge picture only every
     * POWER_METER_SHARED_BUS_DIVIDER ticks. */
    /* While the Linux host is powered nothing is read at all: the device has
     * cut its own power after our reads collided with the host's charger
     * writes (the log ends on such a collision). The LEDs keep their last
     * state until the host is off; live figures under Linux have to come
     * from Linux itself. POWER_METER_SHARED_BUS_READS re-enables the thinned
     * reads for experiments. */
    bool shared = power_host_shares_bus(power);
    if(shared && !POWER_METER_SHARED_BUS_READS) {
        furi_record_close(RECORD_POWER);
        instance->tick++;
        return false;
    }
    bool full_read = shared ? (instance->tick % POWER_METER_SHARED_BUS_DIVIDER == 0) : (instance->tick % POWER_METER_FULL_READ_DIVIDER == 0);
    if(instance->settle_ticks > 0) {
        instance->settle_ticks--;
        full_read = true;
    }
    if(full_read || !instance->charge_valid) {
        PowerChargeInfo charge = {0};
        if(power_read_charge_info(power, &charge)) {
            bool flipped = instance->charge_valid && ((instance->charge.battery_ma > 0) != (charge.battery_ma > 0));
            if(flipped) instance->settle_ticks = POWER_METER_SETTLE_TICKS;
            instance->charge = charge;
            instance->charge_valid = true;
        }
    } else {
        /* The light read: the cable (one status register) and the battery's
         * current and voltage. A cable change refreshes the whole picture at
         * once and opens the settle window. */
        Bq2579xChargerStatusReg status = {0};
        if(power_bq2579x_get_charger_status(power, &status) && !(status.data[0] == 0xFF && status.data[1] == 0xFF)) {
            bool input_present = status.stat0.vbus_present_stat;
            if(input_present != instance->charge.input_present) {
                instance->settle_ticks = POWER_METER_SETTLE_TICKS;
                PowerChargeInfo charge = {0};
                if(power_read_charge_info(power, &charge)) instance->charge = charge;
            }
        }
        int16_t battery_ma = 0;
        uint16_t vbat_mv = 0;
        float battery_v = 0.f;
        if(power_bq2579x_get_ibat_ma(power, &battery_ma) && power_bq2579x_get_vbat_mv(power, &vbat_mv) &&
           power_battery_reading_plausible(battery_ma, battery_v = (float)vbat_mv / 1000.f)) {
            instance->charge.battery_ma = battery_ma;
            instance->charge.battery_v = battery_v;
            instance->charge.discharge_mw = battery_ma < 0 ? (uint32_t)((float)-battery_ma * battery_v) : 0;
        }
    }
    /* Right after the cable goes the charger's ADC reports almost no battery
     * current for a few seconds; the gauge's shunt knows better, so the bars
     * follow it until the charger ADC is back. */
    if(instance->settle_ticks > 0 && instance->charge_valid && !instance->charge.input_present) {
        int16_t gauge_ma = 0;
        float gauge_v = 0.f;
        if(power_bq28z620_get_current(power, &gauge_ma) && power_bq28z620_get_voltage(power, &gauge_v) && power_battery_reading_plausible(gauge_ma, gauge_v)) {
            instance->charge.discharge_mw = gauge_ma < 0 ? (uint32_t)((float)-gauge_ma * gauge_v) : 0;
        }
    }
    furi_record_close(RECORD_POWER);
    instance->tick++;

    if(!instance->charge_valid) return false;
    const PowerChargeInfo* charge = &instance->charge;

    state->in_connected = charge->input_present;
    state->in_pd_valid = charge->input_present && charge->vbus_mv > POWER_METER_PD_VBUS_MV;
    state->error = charge->charger_fault;
    state->charger_charging = (charge->state == PowerChargeStateCharging);
    state->charge_done = (charge->state == PowerChargeStateFull);
    state->battery_percent = charge->soc;
    /* The bars show what the battery gives (IBAT x VBAT), 0 while it charges. */
    state->power_consumption_mw = charge->discharge_mw;
    state->in_power_mw = charge->input_mw;
    return true;
}

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
        FURI_LOG_I(TAG, "battery %lu mW -> level %u", state->power_consumption_mw, consumption_level);
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
    bool is_charging = state->charger_charging;
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
    } else if(state->charge_done) {
        // Fully charged - all off
        color = LED_COLOR_BLACK;
    } else if(is_charging || (state->in_connected && state->power_consumption_mw == 0)) {
        // Charging - all green (also the first seconds after a cable comes, while
        // the charger already carries the load but has not reported charging yet)
        charge_led = true;
        color = LED_COLOR_GREEN;
    } else if(state->in_connected) {
        // Cable in but the battery still gives current: the load eats the input - blink all yellow
        blink = true;
        charge_led = true;
        color = LED_COLOR_YELLOW;
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
        FURI_LOG_I(TAG, "battery led: in=%d charging=%d done=%d -> %s%s", state->in_connected, state->charger_charging, state->charge_done, charge_led ? "full " : "outline ", blink ? "blink" : "steady");
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
    if(!power_meter_read_state(instance, &state)) return;

    LedItem leds[11]; // 11 leds total
    LedBatch batch = {.items = leds, .count = 0};

    update_consumption_leds(instance, &state, &batch);
    update_battery_leds(instance, &state, &batch);
    update_input_leds(instance, &state, &batch);

    if(batch.count > 0) {
        Led* led = furi_record_open(RECORD_LEDS);
        led_set_color_batch(led, &batch);
        furi_record_close(RECORD_LEDS);
    }
}

int32_t power_meter_srv(void* p) {
    UNUSED(p);

    PowerMeter* instance = malloc(sizeof(PowerMeter));
    instance->event_loop = furi_event_loop_alloc();

    /* Every level starts as "unknown" so the first pass writes all the LEDs. */
    instance->in_power_level_last = UINT8_MAX;
    instance->consumption_level_last = UINT8_MAX;
    instance->in_color_last = LED_COLOR_WHITE;
    instance->battery_color_last = LED_COLOR_WHITE;

    instance->update_timer = furi_event_loop_timer_alloc(instance->event_loop, power_meter_update_timer_callback, FuriEventLoopTimerTypePeriodic, instance);
    instance->blink_timer = furi_event_loop_timer_alloc(instance->event_loop, power_meter_blink_timer_callback, FuriEventLoopTimerTypePeriodic, instance);

    furi_event_loop_timer_start(instance->update_timer, POWER_METER_UPDATE_PERIOD_MS);
    furi_event_loop_run(instance->event_loop);

    return 0;
}
