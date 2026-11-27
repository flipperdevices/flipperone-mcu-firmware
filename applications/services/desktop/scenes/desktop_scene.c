#include <assets.h>
#include <gui/gui.h>
#include <gui/clay_helper.h>
#include <gui/modules/elements.h>
#include <gui/modules/parallax.h>
#include "../scene.h"
#include "../desktop_i.h"

#define TAG "Desktop"

/* Home screen: an underwater scene in the artist's coordinates, centred on the
 * 258 px display and lifted by HOME_SCENE_Y. Layers are drawn bottom to top.
 * The scene stands still for now: its speed is meant to follow charging. */
#define HOME_SCENE_WIDTH 256
#define HOME_SCENE_X     1
#define HOME_SCENE_Y     -2

static const Image* const home_bg_frames[] = {&home_bg};
static const Image* const home_ship_frames[] = {&home_ship};
static const Image* const home_fishr_frames[] = {&home_fishr1, &home_fishr2};
static const Image* const home_fishl_frames[] = {&home_fishl1, &home_fishl2};
static const Image* const home_ground_frames[] = {&home_ground};
static const Image* const home_water_top_frames[] = {&home_water_top};
static const Image* const home_foliage_frames[] = {&home_foliage_1, &home_foliage_2};

/* The fish wrap with the width of their wider frame, so the 34 px one does not jitter */
#define HOME_FISH_WIDTH 35

/* The sleeping dolphin: 14 composite frames on the artist's 113 px canvas,
 * three body poses bobbing by a pixel and bubbles rising from the blowhole
 * (right) and the tail (left) that fade as they go. The canvas sits at scene
 * (71, 39), which puts sleepd_1 at (71, 43) as in the scene layout. */
#define HOME_DOLPHIN_X     71
#define HOME_DOLPHIN_Y     39
#define HOME_DOLPHIN_WIDTH 113
#define HOME_DOLPHIN_FPS   2

#define DOLPHIN_1                {.image = &sleepd_1, .x = 0, .y = 4, .alpha = 255}
#define DOLPHIN_2                {.image = &sleepd_2, .x = 1, .y = 5, .alpha = 255}
#define DOLPHIN_3                {.image = &sleepd_3, .x = 1, .y = 6, .alpha = 255}
#define BUBBLE(img, bx, by, pct) {.image = &img, .x = bx, .y = by, .alpha = (uint8_t)((pct) * 255 / 100)}

static const ParallaxSprite home_dolphin_f1[] = {DOLPHIN_1};
static const ParallaxSprite home_dolphin_f2[] = {DOLPHIN_2, BUBBLE(bubble_mid, 99, 19, 100)};
static const ParallaxSprite home_dolphin_f3[] = {DOLPHIN_3, BUBBLE(bubble_mid, 102, 13, 75), BUBBLE(bubble_small, 100, 21, 100)};
static const ParallaxSprite home_dolphin_f4[] = {DOLPHIN_2, BUBBLE(bubble_mid, 100, 6, 55), BUBBLE(bubble_small, 103, 16, 50)};
static const ParallaxSprite home_dolphin_f5[] = {DOLPHIN_1, BUBBLE(bubble_mid, 97, 0, 15), BUBBLE(bubble_small, 102, 5, 5)};
static const ParallaxSprite home_dolphin_f6[] = {DOLPHIN_2, BUBBLE(bubble_small, 100, 21, 100)};
static const ParallaxSprite home_dolphin_f7[] = {DOLPHIN_3, BUBBLE(bubble_small, 104, 13, 60), BUBBLE(bubble_mid, 100, 20, 100)};
static const ParallaxSprite home_dolphin_f8[] = {DOLPHIN_2, BUBBLE(bubble_small, 102, 3, 20), BUBBLE(bubble_mid, 102, 13, 60)};
static const ParallaxSprite home_dolphin_f9[] = {DOLPHIN_1, BUBBLE(bubble_mid, 98, 6, 25)};
static const ParallaxSprite home_dolphin_f10[] = {DOLPHIN_2, BUBBLE(bubble_mid, 6, 32, 100)};
static const ParallaxSprite home_dolphin_f11[] = {DOLPHIN_3, BUBBLE(bubble_mid, 8, 27, 100)};
static const ParallaxSprite home_dolphin_f12[] = {DOLPHIN_2, BUBBLE(bubble_mid, 5, 18, 75)};
static const ParallaxSprite home_dolphin_f13[] = {DOLPHIN_3, BUBBLE(bubble_mid, 8, 8, 50)};
static const ParallaxSprite home_dolphin_f14[] = {DOLPHIN_2, BUBBLE(bubble_mid, 4, 1, 15)};

#define DOLPHIN_FRAME(f) {.sprites = f, .sprite_count = COUNT_OF(f)}

static const ParallaxComposite home_dolphin_frames[] = {
    DOLPHIN_FRAME(home_dolphin_f1),
    DOLPHIN_FRAME(home_dolphin_f2),
    DOLPHIN_FRAME(home_dolphin_f3),
    DOLPHIN_FRAME(home_dolphin_f4),
    DOLPHIN_FRAME(home_dolphin_f5),
    DOLPHIN_FRAME(home_dolphin_f6),
    DOLPHIN_FRAME(home_dolphin_f7),
    DOLPHIN_FRAME(home_dolphin_f8),
    DOLPHIN_FRAME(home_dolphin_f9),
    DOLPHIN_FRAME(home_dolphin_f10),
    DOLPHIN_FRAME(home_dolphin_f11),
    DOLPHIN_FRAME(home_dolphin_f12),
    DOLPHIN_FRAME(home_dolphin_f13),
    DOLPHIN_FRAME(home_dolphin_f14),
};

/* The swimming dolphin: three poses of one tail stroke (down, mid, up, mid),
 * aligned on the eye so the head pitches with the stroke. Its 130 px canvas is
 * centred on the sleeping dolphin's (71 + 113 / 2), mid-water. */
#define HOME_SWIM_X     62
#define HOME_SWIM_Y     47
#define HOME_SWIM_WIDTH 130
#define HOME_SWIM_FPS   4

static const ParallaxSprite home_swim_f1[] = {{.image = &swim1s, .x = 0, .y = 0, .alpha = 255}};
static const ParallaxSprite home_swim_f2[] = {{.image = &swim2s, .x = 4, .y = 4, .alpha = 255}};
static const ParallaxSprite home_swim_f3[] = {{.image = &swim3s, .x = 8, .y = 2, .alpha = 255}};

static const ParallaxComposite home_swim_frames[] = {
    DOLPHIN_FRAME(home_swim_f1),
    DOLPHIN_FRAME(home_swim_f3),
    DOLPHIN_FRAME(home_swim_f2),
    DOLPHIN_FRAME(home_swim_f3),
};

/* Once after the MCU boots, the first time the screen is shown, the dolphin
 * swims in from beyond the left edge to its place and falls asleep there. */
#define HOME_SWIM_IN_FROM  (-(HOME_SWIM_X + HOME_SWIM_WIDTH))
#define HOME_SWIM_IN_SPEED 120.f

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
    {.composites = home_dolphin_frames,
     .frame_count = COUNT_OF(home_dolphin_frames),
     .fps = HOME_DOLPHIN_FPS,
     .x = HOME_DOLPHIN_X,
     .y = HOME_DOLPHIN_Y,
     .wrap = ParallaxWrapNone,
     .wrap_width = HOME_DOLPHIN_WIDTH,
     .face_against_motion = true,
     .show = ParallaxShowStill},
    {.composites = home_swim_frames,
     .frame_count = COUNT_OF(home_swim_frames),
     .fps = HOME_SWIM_FPS,
     .x = HOME_SWIM_X,
     .y = HOME_SWIM_Y,
     .wrap = ParallaxWrapNone,
     .wrap_width = HOME_SWIM_WIDTH,
     .face_against_motion = true,
     .show = ParallaxShowMoving},
    {.composites = home_swim_frames,
     .frame_count = COUNT_OF(home_swim_frames),
     .fps = HOME_SWIM_FPS,
     .x = HOME_SWIM_X,
     .y = HOME_SWIM_Y,
     .wrap = ParallaxWrapNone,
     .wrap_width = HOME_SWIM_WIDTH,
     .show = ParallaxShowArriving,
     .arrive_from = HOME_SWIM_IN_FROM,
     .arrive_speed = HOME_SWIM_IN_SPEED},
};

typedef struct {
    bool help_pressed;
    bool settings_pressed;
    Desktop* desktop;
    Parallax* scene;
} DesktopViewModel;

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
            parallax_set_origin(model->scene, HOME_SCENE_X, HOME_SCENE_Y);
        },
        false);

    view_set_layout_callback(view, desktop_layout);
    view_set_input_callback(view, desktop_input, scene);
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
    .on_enter = NULL,
    .on_exit = desktop_on_exit,
    .on_event = NULL,
};
