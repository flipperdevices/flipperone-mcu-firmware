#include <assets.h>
#include <gui/gui.h>
#include <gui/clay_helper.h>
#include <gui/modules/elements.h>
#include <gui/modules/parallax.h>
#include <power/power.h>
#include <math.h>
#include "../scene.h"
#include "../desktop_i.h"

#define TAG "Desktop"

/* Home screen parallax: an underwater scene laid out on the 256x144 screen
 * (scene coordinates are screen coordinates, centred on the 258 px display).
 * Layers are drawn bottom to top; more arrive as their art lands in
 * assets/home_screen/. */
#define HOME_SCENE_WIDTH 256

static const Image* const home_bg_frames[] = {&home_bg};
static const Image* const home_ship_frames[] = {&home_ship};
static const Image* const home_fishr_frames[] = {&home_fishr1, &home_fishr2};
static const Image* const home_fishl_frames[] = {&home_fishl1, &home_fishl2};
static const Image* const home_ground_frames[] = {&home_ground};
static const Image* const home_water_top_frames[] = {&home_water_top};
static const Image* const home_foliage_frames[] = {&home_foliage_1, &home_foliage_2};

/* The fish wrap with the width of their wider frame, so the 34 px one does not jitter. */
#define HOME_FISH_WIDTH 35

static const ParallaxLayer home_scene_layers[] = {
    {.frames = home_bg_frames, .frame_count = 1, .x = 0, .y = 42, .speed_mult = 0.8f, .wrap = ParallaxWrapTile, .wrap_width = 256},
    {.frames = home_ship_frames, .frame_count = 1, .x = 99, .y = 44, .speed_mult = 0.7f, .wrap = ParallaxWrapPeriod, .wrap_width = 73},
    {.frames = home_fishr_frames,
     .frame_count = 2,
     .fps = 3,
     .x = -HOME_FISH_WIDTH,
     .y = 41,
     .speed_mult = 0.9f,
     .swim = 10.f,
     .wrap = ParallaxWrapPeriod,
     .wrap_width = HOME_FISH_WIDTH},
    {.frames = home_fishl_frames,
     .frame_count = 2,
     .fps = 3,
     .x = HOME_SCENE_WIDTH,
     .y = 73,
     .speed_mult = 0.9f,
     .swim = -10.f,
     .wrap = ParallaxWrapPeriod,
     .wrap_width = HOME_FISH_WIDTH},
    {.frames = home_ground_frames, .frame_count = 1, .x = 0, .y = 99, .speed_mult = 0.8f, .wrap = ParallaxWrapTile, .wrap_width = 256},
    {.frames = home_water_top_frames, .frame_count = 1, .x = 0, .y = 27, .speed_mult = 0.8f, .wrap = ParallaxWrapTile, .wrap_width = 256},
    {.frames = home_foliage_frames, .frame_count = 2, .fps = 2, .x = 0, .y = 84, .speed_mult = 1.0f, .wrap = ParallaxWrapTile, .wrap_width = 256},
};

/* Scene speed from the battery power: charging drags the scene left, up to
 * -100 px/s at 36 W; discharging drags it right, up to +100 px/s at 60 W.
 * Anything that moves goes at least 30 px/s; below HOME_SCENE_IDLE_W the
 * scene stands still. */
#define HOME_SCENE_CHARGE_FULL_W    36.f
#define HOME_SCENE_DISCHARGE_FULL_W 60.f
#define HOME_SCENE_FULL_SPEED       100.f
#define HOME_SCENE_MIN_SPEED        30.f
#define HOME_SCENE_IDLE_W           0.5f

static float desktop_scene_speed_from_power(float watts) {
    float speed;
    if(watts > HOME_SCENE_IDLE_W) {
        speed = -HOME_SCENE_FULL_SPEED * watts / HOME_SCENE_CHARGE_FULL_W;
    } else if(watts < -HOME_SCENE_IDLE_W) {
        speed = HOME_SCENE_FULL_SPEED * -watts / HOME_SCENE_DISCHARGE_FULL_W;
    } else {
        return 0.f;
    }
    return copysignf(fmaxf(fabsf(speed), HOME_SCENE_MIN_SPEED), speed);
}

typedef struct {
    bool help_pressed;
    bool settings_pressed;
    Desktop* desktop;
    Parallax* scene;
} DesktopViewModel;

static void desktop_update_scene_speed(Scene* scene) {
    View* view = scene_get_view(scene);

    /* AveragePower() of the gauge: mW, positive while charging. */
    int16_t power_mw = 0;
    Power* power = furi_record_open(RECORD_POWER);
    bool ok = power_bq28z620_get_average_power(power, &power_mw);
    furi_record_close(RECORD_POWER);
    if(!ok) return;

    float speed = desktop_scene_speed_from_power((float)power_mw / 1000.f);
    with_view_model(view, DesktopViewModel * model, { parallax_set_speed(model->scene, speed); }, true);
}

static bool desktop_layout(void* _model) {
    DesktopViewModel* model = _model;
    furi_assert(model);

    CLAY(
        CLAY_APP_ID("Container"),
        {
            .backgroundColor = COLOR_WHITE,
            .layout =
                {
                    .layoutDirection = CLAY_TOP_TO_BOTTOM,
                    .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)},
                    .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
                },
            .clip = {.vertical = true, .childOffset = Clay_GetScrollOffset()},
        }) {
        CLAY(
            CLAY_APP_ID("FaceWrapper"),
            {
                .layout =
                    {
                        .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)},
                        .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
                    },
            }) {
            parallax_element(CLAY_ID_LOCAL("Scene"), model->scene);
        }

        elements_softkey_button_element(1, "Help", false, model->help_pressed);
        elements_softkey_button_element(2, "Power", false, false);
        elements_softkey_button_element(3, "Settings", false, model->settings_pressed);
    }

    return false;
}

static bool desktop_input(InputEvent* event, void* context) {
    bool consumed = false;
    Desktop* desktop = scene_get_data(context);
    View* view = scene_get_view(context);

    if(event->type == InputTypePress) {
        switch(event->key) {
        case InputKey2:
            with_view_model(view, DesktopViewModel * model, { model->help_pressed = true; }, true);
            consumed = true;
            break;
        case InputKeyPower:
            desktop_send_scene_event(desktop, DesktopSceneEventTypeTogglePowerMenu, NULL);
            consumed = true;
            break;
        case InputKey4:
            with_view_model(view, DesktopViewModel * model, { model->settings_pressed = true; }, true);
            desktop_send_scene_event(desktop, DesktopSceneEventTypeEnterSettingsMenu, context);
            consumed = true;
            break;
        case InputKeySw:
            desktop_send_scene_event(desktop, DesktopSceneEventTypeOpenDebugMenu, NULL);
            consumed = true;
            break;
        default:
            break;
        }
    } else if(event->type == InputTypeRelease) {
        switch(event->key) {
        case InputKey2:
            with_view_model(view, DesktopViewModel * model, { model->help_pressed = false; }, true);
            consumed = true;
            break;
        case InputKey4:
            with_view_model(view, DesktopViewModel * model, { model->settings_pressed = false; }, true);
            consumed = true;
            break;
        default:
            break;
        }
    }

    return consumed;
}

static void desktop_on_alloc(Scene* scene, void* context) {
    View* view = scene_get_view(scene);
    scene_set_data(scene, context);

    view_allocate_model(view, ViewModelTypeLockFree, sizeof(DesktopViewModel));
    with_view_model(
        view,
        DesktopViewModel * model,
        {
            model->desktop = context;
            model->scene = parallax_alloc(home_scene_layers, COUNT_OF(home_scene_layers), HOME_SCENE_WIDTH);
            parallax_set_origin(model->scene, 1, 0);
        },
        false);

    view_set_layout_callback(view, desktop_layout);
    view_set_input_callback(view, desktop_input, scene);
}

static void desktop_on_enter(Scene* scene, void* app) {
    UNUSED(app);
    desktop_update_scene_speed(scene);
}

static bool desktop_on_event(Scene* scene, uint32_t event, void* data) {
    UNUSED(data);
    if(event == DesktopSceneEventTypePowerUpdate) {
        desktop_update_scene_speed(scene);
    }
    /* Not consumed: the header reacts to the same events. */
    return false;
}

static void desktop_on_exit(Scene* scene, void* app) {
    View* view = scene_get_view(scene);

    with_view_model(
        view,
        DesktopViewModel * model,
        {
            model->help_pressed = false;
            model->settings_pressed = false;
        },
        true);
}

const SceneCallbacks scene_desktop_callbacks = {
    .on_alloc = desktop_on_alloc,
    .on_enter = desktop_on_enter,
    .on_exit = desktop_on_exit,
    .on_event = desktop_on_event,
};
