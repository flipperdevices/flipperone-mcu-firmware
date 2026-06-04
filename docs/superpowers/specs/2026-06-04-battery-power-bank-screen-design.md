# Battery (Power Bank) Screen — Design Spec

**Issue:** flipperdevices/flipperone-mcu-firmware#46  
**Date:** 2026-06-04

---

## Overview

When the Flipper One enters OTG/power-bank mode (detected via BQ25792 charger IC), the device automatically shows a dedicated full-screen display with live battery and output stats. The screen dismisses when OTG mode ends or when the user presses Back.

---

## Architecture

### Files changed

| File | Change |
|------|--------|
| `applications/services/power/power.h` | Add `PowerPubSubEventType` enum and `PowerPubSubEvent` struct |
| `applications/services/power/power.c` | Track OTG state in ISR callback; publish events via existing pubsub |
| `applications/main/battery_bank/battery_bank_app.c` | New app — the screen |
| `applications/services/desktop/desktop.c` | Subscribe to power pubsub; auto-launch/stop battery_bank_app |
| `applications/applications.c` | Register `battery_bank_app` in `FLIPPER_APPS[]` |
| `applications/applications.h` | Declare `battery_bank_app` entry point |

---

## Component Design

### 1. Power Service — OTG event publishing

Add to `power.h`:

```c
typedef enum {
    PowerPubSubEventOtgEnabled,
    PowerPubSubEventOtgDisabled,
} PowerPubSubEventType;

typedef struct {
    PowerPubSubEventType type;
} PowerPubSubEvent;
```

Extend `power.c`:

- Add `bool otg_active` field to the `Power` struct to track previous state.
- In `power_custom_event_callback` (which fires on every BQ25792 ISR): call `bq25792_get_charger_status(instance->bq25792_header, &status)` directly on the driver (not the public `power_bq25792_get_charger_status` wrapper, which would deadlock by re-enqueuing a message onto the same event loop). Extract the `vbus_status` field, compare against `Bq25792ChargerStatus1VbusOtg`.
- On transition `false → true`: publish `PowerPubSubEventOtgEnabled`.
- On transition `true → false`: publish `PowerPubSubEventOtgDisabled`.

No new API functions needed — `power_get_pubsub()` already exposes the pubsub to subscribers.

### 2. Battery Bank App

**Location:** `applications/main/battery_bank/battery_bank_app.c`

**Pattern:** identical to the app template — single `View`, `FuriEventLoop`, no ViewDispatcher.

**AppModel fields:**

```c
typedef struct {
    uint8_t  soc_pct;        // BQ28Z620 relative state of charge
    float    vbat_v;         // BQ28Z620 voltage (V)
    int16_t  ibus_ma;        // BQ25792 IBUS (output current, mA)
    uint16_t vbus_mv;        // BQ25792 VBUS (output voltage, mV)
    uint16_t time_to_empty;  // BQ28Z620 average time-to-empty (minutes)
    bool     data_valid;     // false until first successful poll
} BatteryBankModel;
```

**Polling:** A `FuriTimer` fires every 2000 ms. The callback calls `power_bq28z620_get_relative_state_of_charge`, `power_bq28z620_get_voltage`, `power_bq25792_get_ibus_ma`, `power_bq25792_get_vbus_mv`, `power_bq28z620_get_average_time_to_empty` via the public power API. Updates model with `with_view_model(..., true)` to trigger redraw.

**Layout (Clay, 256×144):**

```
+--------------------------------------------------+
|  Power Bank                                      |  <- header row, FontBody
|--------------------------------------------------|
|                                                  |
|                    87%                           |  <- SoC, large FontTitle
|              ████████████░░░░                   |  <- progress bar
|                                                  |
|--------------------------------------------------|
|  5.12V    1350mA    2h 14m                       |  <- footer: VBUS | IBUS | TTE
+--------------------------------------------------+
```

- Header: "Power Bank" left-aligned, FontBody, black on white.
- SoC%: centered, large font (FontTitle or largest available).
- Progress bar: full-width, filled proportional to SoC, 8px tall, rounded corners.
- Footer row: three evenly-spaced stat cells — VBUS (V), IBUS (mA), time-to-empty. FontBody.
- Background: white. Bar fill: black. Text: black.
- `data_valid = false`: show "—" placeholders until first poll completes.

**Input:** Back key (press) → `furi_thread_signal(FuriSignalExit)`. No other keys consumed.

**Lifecycle:**

```c
int32_t battery_bank_app(void* p) {
    App* app = battery_bank_app_alloc();   // opens RECORD_POWER, starts timer
    furi_event_loop_run(app->event_loop);
    battery_bank_app_free(app);            // stops timer, closes RECORD_POWER
    return 0;
}
```

### 3. Desktop — Auto-launch

**Subscribe** to power pubsub during `desktop_alloc()`:

```c
Power* power = furi_record_open(RECORD_POWER);
FuriPubSub* power_pubsub = power_get_pubsub(power);
furi_pubsub_subscribe(power_pubsub, desktop_power_event_callback, desktop);
furi_record_close(RECORD_POWER);
```

**Callback** (`desktop_power_event_callback`):
- Receives `PowerPubSubEvent*`.
- On `PowerPubSubEventOtgEnabled`: enqueue a `DesktopMessageTypeAppStart` with `battery_bank_app` into `app_message_queue` (same path user-launched apps use).
- On `PowerPubSubEventOtgDisabled`: if `desktop->app.running && desktop->app.thread`, call `furi_thread_signal(desktop->app.thread, FuriSignalExit, NULL)`.

The existing `desktop_app_message_logic` and `desktop_do_app_closed` handle the rest — no new code paths needed.

### 4. Registration

`applications.c` — add to `FLIPPER_APPS[]`:

```c
{
    .app = battery_bank_app,
    .name = "Battery",
    .appid = "battery_bank",
    .stack_size = 2048,
    .flags = FlipperInternalApplicationFlagDefault,
},
```

`applications.h` — add `extern int32_t battery_bank_app(void* p);`

---

## Data Flow

```
BQ25792 HW IRQ
    → power_bq25792_event_isr (ISR context)
        → furi_event_loop_set_custom_event(PowerEventTypeIsr)
            → power_custom_event_callback (power service event loop)
                → read charger status, compare OTG bit
                    → furi_pubsub_publish(PowerPubSubEvent)
                        → desktop_power_event_callback (desktop event loop)
                            → enqueue DesktopMessageTypeAppStart
                                → desktop_app_message_logic
                                    → battery_bank_app starts in new thread
                                        → FuriTimer polls power API every 2s
                                        → Clay layout renders live stats
```

---

## Error Handling

- If power service devices are not fully initialized (`power_is_device_initialized` returns false), individual `power_*` API calls return `false`; the model retains previous values (or `data_valid` stays false on first poll). The screen shows "—" for any field that fails.
- If OTG mode ends while the screen is showing, the desktop signals exit — the app's Back-key path and the forced-exit path are identical (`furi_thread_signal(FuriSignalExit)`), so no special teardown is needed.
- If an app is already running when OTG starts, the desktop logs a warning and does not launch the battery screen (existing behavior — mirrors the current "app already running" guard).

---

## Out of Scope

- OTG enable/disable control (issue #41).
- Persistent history or logging of power-bank sessions.
- Haptic or LED feedback on OTG state change.
