# Battery (Power Bank) Screen Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Auto-show a live battery/power-bank stats screen whenever the BQ25792 charger IC enters OTG mode, and dismiss it when OTG stops.

**Architecture:** The power service detects OTG state transitions via its existing ISR callback and publishes `PowerPubSubEvent` events through its pubsub. The desktop subscribes to those events and auto-launches/stops the `battery_bank_app`, which polls the power API on a 2-second timer and renders stats using Clay on the 256×144 display.

**Tech Stack:** C11, FreeRTOS/Furi, Clay UI, BQ25792 (charger), BQ28Z620 (battery gauge), Pico SDK (RP2350), CMake + Ninja

---

## File Map

| File | Action | Responsibility |
|------|--------|----------------|
| `applications/services/power/power.h` | Modify | Add `PowerPubSubEventType` + `PowerPubSubEvent` types |
| `applications/services/power/power.c` | Modify | Track `otg_active`; detect transitions in ISR callback; publish events |
| `applications/main/battery_bank/battery_bank_app.c` | Create | The screen — model, Clay layout, poll timer, input handler |
| `applications/applications.h` | Modify | Declare `battery_bank_app` entry point |
| `applications/applications.c` | Modify | Register `battery_bank_app` in `FLIPPER_APPS[]` |
| `applications/services/desktop/desktop.c` | Modify | Subscribe to power pubsub; add OTG message types; auto-launch/stop app |

---

## Task 1: Add pubsub event types to `power.h`

**Files:**
- Modify: `applications/services/power/power.h`

- [ ] **Step 1: Add the event types**

Open `applications/services/power/power.h`. After the existing `#ifdef __cplusplus extern "C" {` block (around line 10), add before `FuriPubSub* power_get_pubsub`:

```c
typedef enum {
    PowerPubSubEventOtgEnabled,
    PowerPubSubEventOtgDisabled,
} PowerPubSubEventType;

typedef struct {
    PowerPubSubEventType type;
} PowerPubSubEvent;
```

- [ ] **Step 2: Verify it builds**

```bash
cd ~/source/repos/flipperone-mcu-firmware
cmake -B build -DFIRMWARE_TARGET=100 2>&1 | tail -5
ninja -C build 2>&1 | tail -10
```

Expected: no errors. (The build system is cmake+ninja; the first cmake invocation sets up the build dir.)

- [ ] **Step 3: Commit**

```bash
cd ~/source/repos/flipperone-mcu-firmware
git add applications/services/power/power.h
git commit -m "feat(power): add OTG pubsub event types"
```

---

## Task 2: Publish OTG events from the power service

**Files:**
- Modify: `applications/services/power/power.c`

The `power_custom_event_callback` fires whenever `power_bq25792_event_isr` fires (BQ25792 interrupt). We add OTG state tracking here. We call `bq25792_get_charger_status` directly on the driver — NOT `power_bq25792_get_charger_status` — because we're already on the power service event loop and the public wrapper re-enqueues a message that would deadlock.

- [ ] **Step 1: Add `otg_active` to the `Power` struct**

In `power.c`, find `struct Power {` (around line 28) and add one field:

```c
struct Power {
    FuriEventLoop* event_loop;
    FuriPubSub* event_pubsub;
    Bq25792* bq25792_header;
    Ina219* ina219_header;
    Bq28z620* bq28z620_header;
    FuriMessageQueue* message_queue;
    PowerDevice devices;
    bool otg_active;   /* tracks previous OTG state for transition detection */
};
```

- [ ] **Step 2: Update `power_custom_event_callback`**

Find `power_custom_event_callback` (around line 231). Replace the empty `if(events & PowerEventTypeIsr)` block:

```c
static void power_custom_event_callback(uint32_t events, void* context) {
    furi_assert(context);
    Power* instance = (Power*)context;

    if(events & PowerEventTypeIsr) {
        if(!instance->bq25792_header) return;

        Bq25792ChargerStatusReg status = {0};
        if(bq25792_get_charger_status(instance->bq25792_header, &status) != Bq25792StatusOk) return;

        bool otg_now = (status.stat1.vbus_stat == Bq25792ChargerStatus1VbusOtg);
        if(otg_now == instance->otg_active) return;

        instance->otg_active = otg_now;
        PowerPubSubEvent event = {
            .type = otg_now ? PowerPubSubEventOtgEnabled : PowerPubSubEventOtgDisabled,
        };
        furi_pubsub_publish(instance->event_pubsub, &event);
    }
}
```

- [ ] **Step 3: Build**

```bash
ninja -C build 2>&1 | tail -10
```

Expected: builds cleanly. Watch for any "undeclared" errors — if `bq25792_get_charger_status` is not in scope, verify the `#include <drivers/bq25792/bq25792.h>` include is already present at the top of `power.c` (it is, at line 8).

- [ ] **Step 4: Commit**

```bash
git add applications/services/power/power.c
git commit -m "feat(power): detect OTG transitions and publish pubsub events"
```

---

## Task 3: Create `battery_bank_app.c`

**Files:**
- Create: `applications/main/battery_bank/battery_bank_app.c`

The app is a single `View` + `FuriEventLoop`. A periodic timer fires every 2 s, reads five values from the power API, updates the model, triggers a redraw. Clay renders the layout. Back exits.

- [ ] **Step 1: Create the directory and file**

```bash
mkdir -p ~/source/repos/flipperone-mcu-firmware/applications/main/battery_bank
```

Create `applications/main/battery_bank/battery_bank_app.c` with this full content:

```c
#include <furi.h>
#include <gui/gui.h>
#include <gui/clay_helper.h>
#include <power/power.h>

#define TAG "BatteryBankApp"
#define POLL_INTERVAL_MS 2000

typedef struct {
    uint8_t  soc_pct;
    int16_t  ibus_ma;
    uint16_t vbus_mv;
    uint16_t time_to_empty_min;
    bool     data_valid;
} BatteryBankModel;

typedef struct {
    Gui*              gui;
    View*             view;
    FuriEventLoop*    event_loop;
    FuriThread*       thread;
    Power*            power;
    FuriEventLoopTimer* timer;
} BatteryBankApp;

static void battery_bank_format_time(char* buf, size_t len, uint16_t minutes) {
    if(minutes == 0xFFFF) {
        snprintf(buf, len, "--");
    } else {
        snprintf(buf, len, "%uh%02um", minutes / 60, minutes % 60);
    }
}

static bool battery_bank_layout(void* _model) {
    furi_assert(_model);
    BatteryBankModel* model = _model;

    Clay_Sizing expand = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};

    CLAY(
        CLAY_APP_ID("Root"),
        {.backgroundColor = COLOR_WHITE,
         .layout = {
             .layoutDirection = CLAY_TOP_TO_BOTTOM,
             .sizing = expand,
         }}) {

        /* Header */
        CLAY(
            CLAY_APP_ID("Header"),
            {.backgroundColor = COLOR_BLACK,
             .layout = {
                 .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(16)},
                 .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
             }}) {
            CLAY_TEXT(
                CLAY_STRING("Power Bank"),
                CLAY_TEXT_CONFIG({.fontId = FontButton, .textColor = COLOR_WHITE}));
        }

        /* SoC percentage — center */
        CLAY(
            CLAY_APP_ID("SocArea"),
            {.layout = {
                 .sizing = expand,
                 .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
                 .layoutDirection = CLAY_TOP_TO_BOTTOM,
                 .childGap = 6,
             }}) {

            if(!model->data_valid) {
                CLAY_TEXT(
                    CLAY_STRING("Reading..."),
                    CLAY_TEXT_CONFIG({.fontId = FontBody, .textColor = COLOR_BLACK}));
            } else {
                /* SoC number */
                char soc_buf[8];
                snprintf(soc_buf, sizeof(soc_buf), "%u%%", model->soc_pct);
                CLAY_AUTO_ID({.layout = {.childAlignment = {.x = CLAY_ALIGN_X_CENTER}}}) {
                    CLAY_TEXT(
                        clay_helper_string_from_chars(soc_buf),
                        CLAY_TEXT_CONFIG({.fontId = FontButton, .textColor = COLOR_BLACK}));
                }

                /* Progress bar */
                CLAY(
                    CLAY_APP_ID("BarOuter"),
                    {.backgroundColor = COLOR_BLACK,
                     .cornerRadius = CLAY_CORNER_RADIUS(3),
                     .layout = {
                         .sizing = {.width = CLAY_SIZING_FIXED(200), .height = CLAY_SIZING_FIXED(10)},
                         .padding = {1, 1, 1, 1},
                     }}) {
                    uint16_t fill = (uint16_t)(model->soc_pct * 196 / 100);
                    CLAY(
                        CLAY_APP_ID("BarFill"),
                        {.backgroundColor = COLOR_WHITE,
                         .cornerRadius = CLAY_CORNER_RADIUS(2),
                         .layout = {
                             .sizing = {
                                 .width = CLAY_SIZING_FIXED(fill),
                                 .height = CLAY_SIZING_GROW(0),
                             },
                         }}) {}
                }
            }
        }

        /* Footer stats row */
        CLAY(
            CLAY_APP_ID("Footer"),
            {.backgroundColor = COLOR_BLACK,
             .layout = {
                 .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(20)},
                 .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
                 .childGap = 20,
             }}) {

            if(model->data_valid) {
                char vbus_buf[12], ibus_buf[12], tte_buf[12];
                snprintf(vbus_buf, sizeof(vbus_buf), "%u.%02uV", model->vbus_mv / 1000, (model->vbus_mv % 1000) / 10);
                snprintf(ibus_buf, sizeof(ibus_buf), "%dmA", model->ibus_ma);
                battery_bank_format_time(tte_buf, sizeof(tte_buf), model->time_to_empty_min);

                CLAY_TEXT(clay_helper_string_from_chars(vbus_buf), CLAY_TEXT_CONFIG({.fontId = FontBody, .textColor = COLOR_WHITE}));
                CLAY_TEXT(clay_helper_string_from_chars(ibus_buf), CLAY_TEXT_CONFIG({.fontId = FontBody, .textColor = COLOR_WHITE}));
                CLAY_TEXT(clay_helper_string_from_chars(tte_buf),  CLAY_TEXT_CONFIG({.fontId = FontBody, .textColor = COLOR_WHITE}));
            }
        }
    }

    return false;
}

static void battery_bank_poll(void* context) {
    furi_assert(context);
    BatteryBankApp* app = context;

    uint8_t  soc  = 0;
    int16_t  ibus = 0;
    uint16_t vbus = 0;
    uint16_t tte  = 0xFFFF;

    bool ok = true;
    ok &= power_bq28z620_get_relative_state_of_charge(app->power, &soc);
    ok &= power_bq25792_get_ibus_ma(app->power, &ibus);
    ok &= power_bq25792_get_vbus_mv(app->power, &vbus);
    power_bq28z620_get_average_time_to_empty(app->power, &tte); /* best-effort */

    with_view_model(
        app->view,
        BatteryBankModel * model,
        {
            if(ok) {
                model->soc_pct           = soc;
                model->ibus_ma           = ibus;
                model->vbus_mv           = vbus;
                model->time_to_empty_min = tte;
                model->data_valid        = true;
            }
        },
        true);
}

static bool battery_bank_input(InputEvent* event, void* context) {
    furi_assert(context);
    BatteryBankApp* app = context;

    if(event->type == InputTypePress && event->key == InputKeyBack) {
        furi_thread_signal(app->thread, FuriSignalExit, NULL);
        return true;
    }
    return false;
}

static bool battery_bank_input_touch(InputTouchEvent* event, void* context) {
    UNUSED(event);
    UNUSED(context);
    return false;
}

static BatteryBankApp* battery_bank_app_alloc(void) {
    BatteryBankApp* app = malloc(sizeof(BatteryBankApp));

    app->gui        = furi_record_open(RECORD_GUI);
    app->power      = furi_record_open(RECORD_POWER);
    app->event_loop = furi_event_loop_alloc();
    app->thread     = furi_thread_get_current();

    app->view = view_alloc();
    view_allocate_model(app->view, ViewModelTypeLockFree, sizeof(BatteryBankModel));
    view_set_layout_callback(app->view, battery_bank_layout);
    view_set_input_callback(app->view, battery_bank_input, app);
    view_set_input_touch_callback(app->view, battery_bank_input_touch, app);
    gui_add_view(app->gui, app->view, GuiViewPriorityApplication);

    app->timer = furi_event_loop_timer_alloc(
        app->event_loop, battery_bank_poll, FuriEventLoopTimerTypePeriodic, app);
    furi_event_loop_timer_start(app->timer, furi_ms_to_ticks(POLL_INTERVAL_MS));

    /* kick off an immediate poll */
    battery_bank_poll(app);

    return app;
}

static void battery_bank_app_free(BatteryBankApp* app) {
    furi_event_loop_timer_free(app->timer);
    gui_remove_view(app->gui, app->view);
    view_free(app->view);
    furi_event_loop_free(app->event_loop);
    furi_record_close(RECORD_POWER);
    furi_record_close(RECORD_GUI);
    free(app);
}

int32_t battery_bank_app(void* p) {
    UNUSED(p);
    BatteryBankApp* app = battery_bank_app_alloc();
    furi_event_loop_run(app->event_loop);
    battery_bank_app_free(app);
    return 0;
}
```

- [ ] **Step 2: Build**

```bash
ninja -C build 2>&1 | tail -20
```

Expected: compiles cleanly. Common errors to fix:
- `clay_helper_string_from_chars` not declared: verify `#include <gui/clay_helper.h>` is present.
- `CLAY_AUTO_ID` usage: if it doesn't compile, replace with `CLAY_APP_ID("SocLabel")`.
- `furi_ms_to_ticks` not found: check it's in `<furi.h>`; if not, use `pdMS_TO_TICKS(POLL_INTERVAL_MS)`.

- [ ] **Step 3: Commit**

```bash
git add applications/main/battery_bank/battery_bank_app.c
git commit -m "feat(battery-bank): add battery power bank screen app"
```

---

## Task 4: Register the app

**Files:**
- Modify: `applications/applications.h`
- Modify: `applications/applications.c`

- [ ] **Step 1: Declare in `applications.h`**

In `applications/applications.h`, add next to the other `extern` declarations:

```c
extern int32_t battery_bank_app(void* p);
```

- [ ] **Step 2: Register in `applications.c`**

In `applications/applications.c`, add to `FLIPPER_APPS[]` (after `haptic_test_app`):

```c
{
    .app        = battery_bank_app,
    .name       = "Battery",
    .appid      = "battery_bank",
    .stack_size = 2048,
    .flags      = FlipperInternalApplicationFlagDefault,
},
```

- [ ] **Step 3: Build**

```bash
ninja -C build 2>&1 | tail -10
```

Expected: builds cleanly. The app is now launchable from the desktop menu.

- [ ] **Step 4: Commit**

```bash
git add applications/applications.h applications/applications.c
git commit -m "feat(battery-bank): register battery_bank_app in app list"
```

---

## Task 5: Desktop auto-launch on OTG

**Files:**
- Modify: `applications/services/desktop/desktop.c`

The desktop needs to (a) handle two new message types, and (b) subscribe to the power pubsub. The pubsub callback runs on the power service's thread — it must only enqueue a message into the desktop's existing `app_message_queue` (thread-safe), never call desktop functions directly.

- [ ] **Step 1: Add new message types to the `DesktopMessageType` enum**

Find:
```c
typedef enum {
    DesktopMessageTypeAppStart,
    DesktopMessageTypeAppClosed,
} DesktopMessageType;
```

Replace with:
```c
typedef enum {
    DesktopMessageTypeAppStart,
    DesktopMessageTypeAppClosed,
    DesktopMessageTypeOtgEnabled,
    DesktopMessageTypeOtgDisabled,
} DesktopMessageType;
```

- [ ] **Step 2: Add the power pubsub glue callback**

Add this function before `desktop_alloc`:

```c
static void desktop_power_pubsub_glue(const void* message, void* context) {
    furi_assert(context);
    Desktop* desktop = context;
    const PowerPubSubEvent* event = message;

    DesktopMessage msg = {0};
    if(event->type == PowerPubSubEventOtgEnabled) {
        msg.type = DesktopMessageTypeOtgEnabled;
    } else if(event->type == PowerPubSubEventOtgDisabled) {
        msg.type = DesktopMessageTypeOtgDisabled;
    } else {
        return;
    }
    furi_message_queue_put(desktop->app_message_queue, &msg, 0);
}
```

- [ ] **Step 3: Subscribe to power pubsub in `desktop_alloc`**

You need `<power/power.h>` at the top of the file. Add the include if not present:

```c
#include <power/power.h>
```

At the end of `desktop_alloc`, before `return desktop;`, add:

```c
    Power* power = furi_record_open(RECORD_POWER);
    furi_pubsub_subscribe(power_get_pubsub(power), desktop_power_pubsub_glue, desktop);
    furi_record_close(RECORD_POWER);
```

- [ ] **Step 4: Handle OTG messages in `desktop_app_message_logic`**

Find the `switch(message.type)` in `desktop_app_message_logic`. Add two cases:

```c
    case DesktopMessageTypeOtgEnabled:
        if(!desktop->app.running) {
            desktop->app.running = true;
            desktop_start_internal_app(desktop, &FLIPPER_APPS[/* battery_bank index */], NULL);
        }
        break;
    case DesktopMessageTypeOtgDisabled:
        if(desktop->app.running && desktop->app.thread) {
            furi_thread_signal(desktop->app.thread, FuriSignalExit, NULL);
        }
        break;
```

For the `battery_bank` index: count the entries in `FLIPPER_APPS[]` in `applications.c`. If `battery_bank_app` is the 6th entry (0-indexed: 5), use `&FLIPPER_APPS[5]`. To avoid fragility, add a named lookup. The safer approach: define a helper at the top of `desktop.c`:

```c
static const FlipperInternalApplication* desktop_find_app(const char* appid) {
    for(size_t i = 0; i < FLIPPER_APPS_COUNT; i++) {
        if(strcmp(FLIPPER_APPS[i].appid, appid) == 0) return &FLIPPER_APPS[i];
    }
    return NULL;
}
```

Then use:
```c
    case DesktopMessageTypeOtgEnabled: {
        const FlipperInternalApplication* app = desktop_find_app("battery_bank");
        if(app && !desktop->app.running) {
            desktop->app.running = true;
            desktop_start_internal_app(desktop, app, NULL);
        }
        break;
    }
```

- [ ] **Step 5: Build**

```bash
ninja -C build 2>&1 | tail -20
```

Expected: full clean build. Common issues:
- `desktop_start_internal_app` not declared in this scope: look for it declared as `static` earlier in `desktop.c`; ensure the new function is placed after it, or move `desktop_power_pubsub_glue` after it.
- `strcmp` not in scope: add `#include <string.h>`.

- [ ] **Step 6: Commit**

```bash
git add applications/services/desktop/desktop.c
git commit -m "feat(desktop): auto-launch battery bank screen on OTG enable/disable"
```

---

## Task 6: Push and open PR

- [ ] **Step 1: Push to fork**

```bash
cd ~/source/repos/flipperone-mcu-firmware
git push origin dev
```

- [ ] **Step 2: Open PR**

```bash
gh pr create \
  --repo githouson/flipperone-mcu-firmware \
  --base dev \
  --title "feat: battery power bank screen (closes #46)" \
  --body "$(cat <<'EOF'
## Summary
- Adds `PowerPubSubEvent` (OTG enabled/disabled) published from BQ25792 ISR callback
- Adds `battery_bank_app`: live screen showing SoC%, VBUS, IBUS, time-to-empty, refreshed every 2 s
- Desktop auto-launches the screen when OTG mode starts; dismisses it when OTG stops

## Test plan
- [ ] Build passes with `ninja -C build`
- [ ] With device in OTG mode, battery bank screen appears automatically
- [ ] Stats update every ~2 seconds
- [ ] Back button dismisses screen while OTG is still active
- [ ] Disconnecting OTG source dismisses screen automatically
- [ ] No crash when power service devices are partially initialized (shows "Reading...")

Closes #46
EOF
)"
```
