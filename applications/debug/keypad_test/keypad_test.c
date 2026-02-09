#include <furi.h>
#include <gui/gui.h>
#include <gui/clay_helper.h>

#define TAG "KeypadTestApp"

#define KEYPAD_TEST_INPUT_QUEUE_SIZE 16
#define KEYPAD_TEST_TICKS_TO_EXIT    10
#define KEYPAD_TEST_BUTTON_WIDTH     CLAY_SIZING_FIXED(40)

typedef struct {
    Gui* gui;
    ViewPort* view_port;

    FuriEventLoop* event_loop;
    FuriMessageQueue* input_queue;

    uint32_t key_state;
    size_t exit_counter;
    FuriString* exit_text;
} KeypadTestApp;

static void keypad_test_app_create_empty(void) {
    CLAY_AUTO_ID({
        .layout =
            {
                .padding = {8, 8, 4, 4},
                .sizing = {.width = KEYPAD_TEST_BUTTON_WIDTH},
                .childAlignment = {.x = CLAY_ALIGN_X_CENTER},
            },
        .backgroundColor = COLOR_WHITE,
        .cornerRadius = CLAY_CORNER_RADIUS(4),
    }) {
    }
}

static void keypad_test_app_create_keypad_button(Clay_String text, bool inverted) {
    CLAY_AUTO_ID({
        .border = {.color = COLOR_BLACK, .width = {.top = 1, .left = 1, .right = 1, .bottom = 1}},
        .layout =
            {
                .padding = {8, 8, 4, 4},
                .sizing = {.width = KEYPAD_TEST_BUTTON_WIDTH},
                .childAlignment = {.x = CLAY_ALIGN_X_CENTER},
            },
        .backgroundColor = inverted ? COLOR_WHITE : COLOR_BLACK,
        .cornerRadius = CLAY_CORNER_RADIUS(4),
    }) {
        CLAY_TEXT(text, CLAY_TEXT_CONFIG({.fontId = FontButton, .textColor = inverted ? COLOR_BLACK : COLOR_WHITE}));
    }
}

static void keypad_test_app_layout(void* context) {
    furi_assert(context);
    KeypadTestApp* instance = (KeypadTestApp*)context;

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
                keypad_test_app_create_keypad_button(CLAY_STRING("PTT"), instance->key_state & InputKeyPtt);
                keypad_test_app_create_empty();
                keypad_test_app_create_keypad_button(CLAY_STRING("Up"), instance->key_state & InputKeyUp);
                keypad_test_app_create_empty();
                keypad_test_app_create_empty();
            }
            CLAY_AUTO_ID({.layout = layout_row}) {
                keypad_test_app_create_keypad_button(CLAY_STRING("Left"), instance->key_state & InputKeyLeft);
                keypad_test_app_create_keypad_button(CLAY_STRING("Ok"), instance->key_state & InputKeyOk);
                keypad_test_app_create_keypad_button(CLAY_STRING("Right"), instance->key_state & InputKeyRight);
            }
            CLAY_AUTO_ID({.layout = layout_row}) {
                keypad_test_app_create_keypad_button(CLAY_STRING("Down"), instance->key_state & InputKeyDown);
            }
            CLAY_AUTO_ID({.layout = layout_row}) {
                keypad_test_app_create_keypad_button(CLAY_STRING("SW"), instance->key_state & InputKeySw);
                keypad_test_app_create_empty();
                keypad_test_app_create_empty();
                keypad_test_app_create_empty();
                if(instance->exit_counter > 0) {
                    keypad_test_app_create_keypad_button(clay_helper_string_from(instance->exit_text), instance->key_state & InputKeyBack);
                } else {
                    keypad_test_app_create_keypad_button(CLAY_STRING("Back"), instance->key_state & InputKeyBack);
                }
            }
            CLAY_AUTO_ID({.layout = layout_row}) {
                keypad_test_app_create_keypad_button(CLAY_STRING("1"), instance->key_state & InputKey1);
                keypad_test_app_create_keypad_button(CLAY_STRING("2"), instance->key_state & InputKey2);
                keypad_test_app_create_keypad_button(CLAY_STRING("P"), instance->key_state & InputKey3);
                keypad_test_app_create_keypad_button(CLAY_STRING("4"), instance->key_state & InputKey4);
                keypad_test_app_create_keypad_button(CLAY_STRING("5"), instance->key_state & InputKey5);
            }
        }
    }
}

static void keypad_test_app_input_logic(FuriEventLoopObject* object, void* context) {
    furi_check(context);
    KeypadTestApp* instance = context;
    furi_check(object == instance->input_queue);

    InputEvent event;
    furi_check(furi_message_queue_get(instance->input_queue, &event, 0) == FuriStatusOk);

    if(event.type == InputTypePress || event.type == InputTypeRelease) {
        if(event.type == InputTypePress) {
            instance->key_state |= event.key;
        } else {
            instance->key_state &= ~event.key;
        }
        view_port_update(instance->view_port);
    }

    if(event.key == InputKeyBack) {
        if(event.type == InputTypePress) {
            instance->exit_counter = 1;
            furi_string_printf(instance->exit_text, "%zu", KEYPAD_TEST_TICKS_TO_EXIT);
            view_port_update(instance->view_port);
        } else if(event.type == InputTypeRelease) {
            instance->exit_counter = 0;
            furi_string_set(instance->exit_text, "");
            view_port_update(instance->view_port);
        } else if(event.type == InputTypeRepeat) {
            instance->exit_counter++;
            furi_string_printf(instance->exit_text, "%zu", KEYPAD_TEST_TICKS_TO_EXIT - instance->exit_counter);
            if(instance->exit_counter >= KEYPAD_TEST_TICKS_TO_EXIT) {
                furi_thread_signal(furi_thread_get_current(), FuriSignalExit, NULL);
            }
            view_port_update(instance->view_port);
        }
    }
}

static KeypadTestApp* keypad_test_app_alloc(void) {
    KeypadTestApp* instance = malloc(sizeof(KeypadTestApp));
    instance->gui = furi_record_open(RECORD_GUI);
    instance->event_loop = furi_event_loop_alloc();
    instance->input_queue = furi_message_queue_alloc(KEYPAD_TEST_INPUT_QUEUE_SIZE, sizeof(InputEvent));
    instance->exit_text = furi_string_alloc();

    instance->view_port = view_port_alloc();
    view_port_set_layout_callback(instance->view_port, keypad_test_app_layout, instance);
    view_port_set_input_callback(instance->view_port, view_port_input_queue_glue, instance->input_queue);
    furi_event_loop_subscribe_message_queue(instance->event_loop, instance->input_queue, FuriEventLoopEventIn, keypad_test_app_input_logic, instance);
    gui_add_view_port(instance->gui, instance->view_port, GuiViewPriorityApplication);
    return instance;
}

static void keypad_test_app_free(KeypadTestApp* instance) {
    gui_remove_view_port(instance->gui, instance->view_port);
    furi_record_close(RECORD_GUI);

    furi_event_loop_unsubscribe(instance->event_loop, instance->input_queue);

    view_port_free(instance->view_port);
    furi_message_queue_free(instance->input_queue);
    furi_event_loop_free(instance->event_loop);
    furi_string_free(instance->exit_text);
    free(instance);
}

int32_t keypad_test_app(void* p) {
    KeypadTestApp* instance = keypad_test_app_alloc();
    furi_event_loop_run(instance->event_loop);
    keypad_test_app_free(instance);
    return 0;
}
