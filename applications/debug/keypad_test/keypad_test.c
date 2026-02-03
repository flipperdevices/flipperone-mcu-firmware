#include <furi.h>
#include <gui/gui.h>
#include <gui/clay_helper.h>

#define TAG "KeypadTestApp"

#define APP_INPUT_QUEUE_SIZE 16

#define APP_TICKS_TO_EXIT 10

#define APP_BUTTON_WIDTH CLAY_SIZING_FIXED(40)

typedef struct {
    Gui* gui;
    ViewPort* view_port;

    FuriEventLoop* event_loop;
    FuriMessageQueue* input_queue;

    uint32_t key_state;
    size_t exit_counter;
    FuriString* exit_text;
} App;

static void app_create_empty(void) {
    CLAY_AUTO_ID({
        .layout =
            {
                .padding = {8, 8, 4, 4},
                .sizing = {.width = APP_BUTTON_WIDTH},
                .childAlignment = {.x = CLAY_ALIGN_X_CENTER},
            },
        .backgroundColor = COLOR_WHITE,
        .cornerRadius = CLAY_CORNER_RADIUS(4),
    }) {
    }
}

static void app_create_keypad_button(Clay_String text, bool inverted) {
    CLAY_AUTO_ID({
        .border = {.color = COLOR_BLACK, .width = {.top = 1, .left = 1, .right = 1, .bottom = 1}},
        .layout =
            {
                .padding = {8, 8, 4, 4},
                .sizing = {.width = APP_BUTTON_WIDTH},
                .childAlignment = {.x = CLAY_ALIGN_X_CENTER},
            },
        .backgroundColor = inverted ? COLOR_WHITE : COLOR_BLACK,
        .cornerRadius = CLAY_CORNER_RADIUS(4),
    }) {
        CLAY_TEXT(text, CLAY_TEXT_CONFIG({.fontId = FontButton, .textColor = inverted ? COLOR_BLACK : COLOR_WHITE}));
    }
}

static void app_layout(void* context) {
    furi_assert(context);
    App* app = (App*)context;

    Clay_Sizing layout_expand = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};
    Clay_LayoutConfig layout_row = {
        .sizing = {.height = CLAY_SIZING_FIXED(14), .width = CLAY_SIZING_GROW(0)},
        .childGap = 8,
        .childAlignment =
            {
                .y = CLAY_ALIGN_Y_CENTER,
                .x = CLAY_ALIGN_X_CENTER,
            },
    };

    CLAY(
        CLAY_APP_ID(TAG "Outer"),
        {
            .backgroundColor = COLOR_WHITE,
            .layout =
                {
                    .layoutDirection = CLAY_TOP_TO_BOTTOM,
                    .sizing = layout_expand,
                    .padding = {4, 4, 4, 3},
                    .childGap = 4,
                },
        }) {
        CLAY(
            CLAY_APP_ID("Header"),
            {
                .layout =
                    {
                        .sizing = {.height = CLAY_SIZING_FIXED(14), .width = CLAY_SIZING_GROW(0)},
                        .childGap = 8,
                        .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
                    },
            }) {
            CLAY_AUTO_ID({.layout = {.padding = {8, 8, 4, 4}}}) {
                CLAY_TEXT(CLAY_STRING("Keypad Test"), CLAY_TEXT_CONFIG({.fontId = FontButton, .textColor = COLOR_BLACK}));
            }
        }
        CLAY(
            CLAY_APP_ID("MainContent"),
            {
                .border = {.color = COLOR_BLACK, .width = {.top = 1, .left = 1, .right = 1, .bottom = 1}},
                .cornerRadius = CLAY_CORNER_RADIUS(4),
                .clip = {.vertical = true},
                .layout =
                    {
                        .layoutDirection = CLAY_TOP_TO_BOTTOM,
                        .childGap = 8,
                        .padding = {6, 6, 6, 6},
                        .sizing = layout_expand,
                        .childAlignment = {.y = CLAY_ALIGN_Y_CENTER},
                    },
            }) {
            CLAY_AUTO_ID({.layout = layout_row}) {
                test_create_keypad_button(CLAY_STRING("PTT"), app->key_state & InputKeyPtt);
                test_create_empty();
                test_create_keypad_button(CLAY_STRING("Up"), app->key_state & InputKeyUp);
                test_create_empty();
                test_create_empty();
            }
            CLAY_AUTO_ID({.layout = layout_row}) {
                test_create_keypad_button(CLAY_STRING("Left"), app->key_state & InputKeyLeft);
                test_create_keypad_button(CLAY_STRING("Ok"), app->key_state & InputKeyOk);
                test_create_keypad_button(CLAY_STRING("Right"), app->key_state & InputKeyRight);
            }
            CLAY_AUTO_ID({.layout = layout_row}) {
                test_create_keypad_button(CLAY_STRING("Down"), app->key_state & InputKeyDown);
            }
            CLAY_AUTO_ID({.layout = layout_row}) {
                test_create_keypad_button(CLAY_STRING("SW"), app->key_state & InputKeySw);
                test_create_empty();
                test_create_empty();
                test_create_empty();
                if(app->exit_counter > 0) {
                    test_create_keypad_button(clay_helper_string_from(app->exit_text), app->key_state & InputKeyBack);
                } else {
                    test_create_keypad_button(CLAY_STRING("Back"), app->key_state & InputKeyBack);
                }
            }
            CLAY_AUTO_ID({.layout = layout_row}) {
                test_create_keypad_button(CLAY_STRING("1"), app->key_state & InputKey1);
                test_create_keypad_button(CLAY_STRING("2"), app->key_state & InputKey2);
                test_create_keypad_button(CLAY_STRING("P"), app->key_state & InputKey3);
                test_create_keypad_button(CLAY_STRING("4"), app->key_state & InputKey4);
                test_create_keypad_button(CLAY_STRING("5"), app->key_state & InputKey5);
            }
        }
    }
}

static void app_input_logic(FuriEventLoopObject* object, void* context) {
    furi_check(context);
    App* app = context;
    furi_check(object == app->input_queue);

    InputEvent event;
    furi_check(furi_message_queue_get(app->input_queue, &event, 0) == FuriStatusOk);

    if(event.type == InputTypePress || event.type == InputTypeRelease) {
        if(event.type == InputTypePress) {
            app->key_state |= event.key;
        } else {
            app->key_state &= ~event.key;
        }
        gui_update(app->gui);
    }

    if(event.key == InputKeyBack) {
        if(event.type == InputTypePress) {
            app->exit_counter = 1;
            furi_string_printf(app->exit_text, "%zu", APP_TICKS_TO_EXIT);
            gui_update(app->gui);
        } else if(event.type == InputTypeRelease) {
            app->exit_counter = 0;
            furi_string_set(app->exit_text, "");
            gui_update(app->gui);
        } else if(event.type == InputTypeRepeat) {
            app->exit_counter++;
            furi_string_printf(app->exit_text, "%zu", APP_TICKS_TO_EXIT - app->exit_counter);
            if(app->exit_counter >= APP_TICKS_TO_EXIT) {
                furi_thread_signal(furi_thread_get_current(), FuriSignalExit, NULL);
            }
            gui_update(app->gui);
        }
    }
}

static App* app_alloc(void) {
    App* app = malloc(sizeof(App));
    app->gui = furi_record_open(RECORD_GUI);
    app->event_loop = furi_event_loop_alloc();
    app->input_queue = furi_message_queue_alloc(APP_INPUT_QUEUE_SIZE, sizeof(InputEvent));
    app->exit_text = furi_string_alloc();

    app->view_port = view_port_alloc();
    view_port_set_layout_callback(app->view_port, app_layout, app);
    view_port_set_input_callback(app->view_port, view_port_input_queue_glue, app->input_queue);
    furi_event_loop_subscribe_message_queue(app->event_loop, app->input_queue, FuriEventLoopEventIn, app_input_logic, app);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);
    return app;
}

static void app_free(App* app) {
    gui_remove_view_port(app->gui, app->view_port);
    furi_record_close(RECORD_GUI);

    furi_event_loop_unsubscribe(app->event_loop, app->input_queue);

    view_port_free(app->view_port);
    furi_message_queue_free(app->input_queue);
    furi_event_loop_free(app->event_loop);
    furi_string_free(app->exit_text);
    free(app);
}

int32_t keypad_test_app(void* p) {
    App* app = app_alloc();
    furi_event_loop_run(app->event_loop);
    app_free(app);
    return 0;
}
