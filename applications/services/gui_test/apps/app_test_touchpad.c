#include <gui_test/app_common.h>

#define TAG "TouchpadTest"

typedef struct {
    uint32_t x;
    uint32_t y;
    bool pressed;
} TouchpadState;

static void test_touchpad_layout(App* app) {
    furi_assert(app);
    TouchpadState* state = (TouchpadState*)app->state;

    Clay_Sizing layoutExpand = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};
    Clay_BorderElementConfig contentBorders = {.color = COLOR_BLACK, .width = {.top = 1, .left = 1, .right = 1, .bottom = 1}};

    float touch_x = (state->x - 180.f) / 2.8f;
    float touch_y = (state->y - 180.f) / 5.f;

    CLAY(
        CLAY_APP_ID("OuterContainer"),
        {.backgroundColor = COLOR_WHITE,
         .layout = {
             .layoutDirection = CLAY_TOP_TO_BOTTOM,
             .sizing = layoutExpand,
             .padding = {4, 4, 4, 3},
             .childGap = 4,
         }}) {
        // Child elements go inside braces
        CLAY(
            CLAY_APP_ID("HeaderBar"),
            {
                .layout =
                    {.sizing = {.height = CLAY_SIZING_FIXED(14), .width = CLAY_SIZING_GROW(0)},
                     .childGap = 8,
                     .childAlignment =
                         {
                             .y = CLAY_ALIGN_Y_CENTER,
                         }},
            }) {
            // Header buttons go here
            CLAY_AUTO_ID({.layout = {.padding = {8, 8, 4, 4}}, .backgroundColor = COLOR_BLACK, .cornerRadius = CLAY_CORNER_RADIUS(4)}) {
                CLAY_TEXT(
                    CLAY_STRING("Touchpad Test"),
                    CLAY_TEXT_CONFIG({
                        .fontId = FontButton,
                        .textColor = COLOR_WHITE,
                    }));
            }
        }
        // CLAY_AUTO_ID({.layout = {.sizing = {CLAY_SIZING_GROW(0)}}}) {
        //             }
        CLAY(
            CLAY_APP_ID("LowerContent"),
            {
                .layout =
                    {
                        .sizing = layoutExpand,
                        .childGap = 4,
                    },
            }) {
            CLAY(
                CLAY_APP_ID("MainContent"),
                {
                    .border = contentBorders,
                    .cornerRadius = CLAY_CORNER_RADIUS(60),
                    .clip = {.vertical = true},
                    .layout =
                        {
                            .layoutDirection = CLAY_TOP_TO_BOTTOM,
                            .childGap = 8,
                            .padding = {6, 6, 6, 6},
                            .sizing = layoutExpand,
                            .childAlignment =
                                {
                                    .y = CLAY_ALIGN_Y_TOP,
                                    .x = CLAY_ALIGN_X_LEFT,
                                },
                        },
                }) {
                CLAY_AUTO_ID({
                    .border = {.color = COLOR_BLACK, .width = {1, 1, 1, 1}},
                    .floating =
                        {
                            .offset = {.x = touch_x, .y = touch_y},
                            .attachTo = CLAY_ATTACH_TO_PARENT,
                        },
                    .layout =
                        {
                            .padding = {8, 8, 4, 4},
                            .sizing = {.width = CLAY_SIZING_FIXED(0), .height = CLAY_SIZING_FIXED(15)},
                        },
                    .backgroundColor = state->pressed ? COLOR_BLACK : COLOR_WHITE,
                    .cornerRadius = CLAY_CORNER_RADIUS(4),
                }) {
                }
            }
        }
    }
}

static bool test_touchpad_input(App* app, const GuiTestMessage* message) {
    furi_assert(app);
    furi_assert(message);

    TouchpadState* state = (TouchpadState*)app->state;
    bool handled = false;

    switch(message->type) {
    case GuiTestMessageTypeInputEvent: {
    } break;
    case GuiTestMessageTypeInputTouchEvent: {
        InputTouchEvent event = message->input_touch_event;

        switch(event.type) {
        case InputTouchTypeStart:
            state->pressed = true;
            state->x = event.x;
            state->y = event.y;
            handled = true;
            break;
        case InputTouchTypeMove:
            state->x = event.x;
            state->y = event.y;
            handled = true;
            break;
        case InputTouchTypeEnd:
            state->pressed = false;
            handled = true;
            break;
        }
    } break;
    }

    return handled;
}

App app_test_touchpad = {
    .state = &(TouchpadState){0},
    .input = test_touchpad_input,
    .render = test_touchpad_layout,
};
