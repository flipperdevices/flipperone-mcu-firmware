#include <gui_test/app_common.h>

#define TAG "KeypadTest"

typedef struct {
    uint32_t key_state;
} KeypadState;

static void test_create_empty(void) {
    CLAY_AUTO_ID(
        {.layout =
             {
                 .padding = {8, 8, 4, 4},
                 .sizing = {.width = CLAY_SIZING_FIXED(40)},
                 .childAlignment =
                     {
                         .y = CLAY_ALIGN_Y_CENTER,
                         .x = CLAY_ALIGN_X_CENTER,
                     },
             },
         .backgroundColor = COLOR_WHITE,
         .cornerRadius = CLAY_CORNER_RADIUS(4)}) {
    }
}

static void test_create_keypad_button(Clay_String text, bool inverted) {
    CLAY_AUTO_ID(
        {.border = {.color = COLOR_BLACK, .width = {.top = 1, .left = 1, .right = 1, .bottom = 1}},
         .layout =
             {
                 .padding = {8, 8, 4, 4},
                 .sizing = {.width = CLAY_SIZING_FIXED(40)},
                 .childAlignment =
                     {
                         .y = CLAY_ALIGN_Y_CENTER,
                         .x = CLAY_ALIGN_X_CENTER,
                     },
             },
         .backgroundColor = inverted ? COLOR_WHITE : COLOR_BLACK,
         .cornerRadius = CLAY_CORNER_RADIUS(4)}) {
        CLAY_TEXT(
            text,
            CLAY_TEXT_CONFIG({
                .fontId = FontButton,
                .textColor = inverted ? COLOR_BLACK : COLOR_WHITE,
            }));
    }
}

static void test_keypad_layout(App* app) {
    furi_assert(app);
    KeypadState* state = (KeypadState*)app->state;

    Clay_Sizing layoutExpand = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};
    Clay_BorderElementConfig contentBorders = {.color = COLOR_BLACK, .width = {.top = 1, .left = 1, .right = 1, .bottom = 1}};

    CLAY(
        CLAY_APP_ID(TAG "OuterContainer"),
        {
            .backgroundColor = COLOR_WHITE,
            .layout =
                {
                    .layoutDirection = CLAY_TOP_TO_BOTTOM,
                    .sizing = layoutExpand,
                    .padding = {4, 4, 4, 3},
                    .childGap = 4,
                },
        }) {
        // Child elements go inside braces
        CLAY(
            CLAY_APP_ID("HeaderBar"),
            {
                .layout =
                    {
                        .sizing = {.height = CLAY_SIZING_FIXED(14), .width = CLAY_SIZING_GROW(0)},
                        .childGap = 8,
                        .childAlignment = {.y = CLAY_ALIGN_Y_CENTER},
                    },
            }) {
            // Header buttons go here
            CLAY_AUTO_ID({.layout = {.padding = {8, 8, 4, 4}}, .backgroundColor = COLOR_BLACK, .cornerRadius = CLAY_CORNER_RADIUS(4)}) {
                CLAY_TEXT(
                    CLAY_STRING("Keypad Test"),
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
                    .cornerRadius = CLAY_CORNER_RADIUS(4),
                    .clip = {.vertical = true},
                    .layout =
                        {
                            .layoutDirection = CLAY_TOP_TO_BOTTOM,
                            .childGap = 8,
                            .padding = {6, 6, 6, 6},
                            .sizing = layoutExpand,
                            .childAlignment =
                                {
                                    .y = CLAY_ALIGN_Y_CENTER,
                                    .x = CLAY_ALIGN_X_CENTER,
                                },
                        },
                }) {
                CLAY_AUTO_ID({
                    .layout =
                        {
                            .sizing = {.height = CLAY_SIZING_FIXED(14), .width = CLAY_SIZING_GROW(0)},
                            .childGap = 8,
                            .childAlignment =
                                {
                                    .y = CLAY_ALIGN_Y_CENTER,
                                    .x = CLAY_ALIGN_X_CENTER,
                                },
                        },
                }) {
                    test_create_keypad_button(CLAY_STRING("PTT"), state->key_state & InputKeyPtt);
                    test_create_empty();
                    test_create_keypad_button(CLAY_STRING("Up"), state->key_state & InputKeyUp);
                    test_create_empty();
                    test_create_empty();
                }
                CLAY_AUTO_ID({
                    .layout =
                        {
                            .sizing = {.height = CLAY_SIZING_FIXED(14), .width = CLAY_SIZING_GROW(0)},
                            .childGap = 8,
                            .childAlignment =
                                {
                                    .y = CLAY_ALIGN_Y_CENTER,
                                    .x = CLAY_ALIGN_X_CENTER,
                                },
                        },
                }) {
                    test_create_keypad_button(CLAY_STRING("Left"), state->key_state & InputKeyLeft);
                    test_create_keypad_button(CLAY_STRING("Ok"), state->key_state & InputKeyOk);
                    test_create_keypad_button(CLAY_STRING("Right"), state->key_state & InputKeyRight);
                }
                CLAY_AUTO_ID({
                    .layout =
                        {
                            .sizing = {.height = CLAY_SIZING_FIXED(14), .width = CLAY_SIZING_GROW(0)},
                            .childGap = 8,
                            .childAlignment =
                                {
                                    .y = CLAY_ALIGN_Y_CENTER,
                                    .x = CLAY_ALIGN_X_CENTER,
                                },
                        },
                }) {
                    test_create_keypad_button(CLAY_STRING("Down"), state->key_state & InputKeyDown);
                }
                CLAY_AUTO_ID({
                    .layout =
                        {
                            .sizing = {.height = CLAY_SIZING_FIXED(14), .width = CLAY_SIZING_GROW(0)},
                            .childGap = 8,
                            .childAlignment =
                                {
                                    .y = CLAY_ALIGN_Y_CENTER,
                                    .x = CLAY_ALIGN_X_CENTER,
                                },
                            .padding = {0, 0, 0, 0},
                        },
                }) {
                    test_create_empty();
                    test_create_empty();
                    test_create_empty();
                    test_create_empty();
                    test_create_keypad_button(CLAY_STRING("Back"), state->key_state & InputKeyBack);
                }
                CLAY_AUTO_ID({
                    .layout =
                        {
                            .sizing = {.height = CLAY_SIZING_FIXED(14), .width = CLAY_SIZING_GROW(0)},
                            .childGap = 8,
                            .childAlignment =
                                {
                                    .y = CLAY_ALIGN_Y_CENTER,
                                    .x = CLAY_ALIGN_X_CENTER,
                                },
                        },
                }) {
                    test_create_keypad_button(CLAY_STRING("1"), state->key_state & InputKey1);
                    test_create_keypad_button(CLAY_STRING("2"), state->key_state & InputKey2);
                    test_create_keypad_button(CLAY_STRING("P"), state->key_state & InputKey3);
                    test_create_keypad_button(CLAY_STRING("4"), state->key_state & InputKey4);
                    test_create_keypad_button(CLAY_STRING("5"), state->key_state & InputKey5);
                }
            }
        }
    }
}

static bool test_keypad_input(App* app, const GuiTestMessage* message) {
    furi_assert(app);
    furi_assert(message);

    KeypadState* state = (KeypadState*)app->state;
    bool handled = false;

    switch(message->type) {
    case GuiTestMessageTypeInputEvent: {
        InputEvent event = message->input_event;
        if(event.type == InputTypePress || event.type == InputTypeRelease) {
            if(event.key != InputKeySw) {
                if(event.type == InputTypePress) {
                    state->key_state |= event.key;
                } else {
                    state->key_state &= ~event.key;
                }
                handled = true;
            }
        }
    } break;
    case GuiTestMessageTypeInputTouchEvent:
        break;
    }

    return handled;
}

App app_test_keypad = {
    .state = &(KeypadState){0},
    .input = test_keypad_input,
    .render = test_keypad_layout,
};
