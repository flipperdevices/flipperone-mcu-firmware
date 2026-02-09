#include <furi.h>
#include <gui/gui.h>
#include <gui/clay_helper.h>

#define TAG "KeypadTestApp"

#define KEYPAD_TEST_TICKS_TO_EXIT 10
#define KEYPAD_TEST_BUTTON_WIDTH  CLAY_SIZING_FIXED(40)

typedef struct {
    uint32_t key_state;
    size_t exit_counter;
    FuriString* exit_text;
} KeypadTestModel;

typedef struct {
    Gui* gui;
    ViewPort* view_port;
    FuriEventLoop* event_loop;
    FuriThread* thread;
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

static bool keypad_test_app_layout(void* _model) {
    furi_assert(_model);
    KeypadTestModel* model = _model;

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
                keypad_test_app_create_keypad_button(CLAY_STRING("PTT"), model->key_state & InputKeyPtt);
                keypad_test_app_create_empty();
                keypad_test_app_create_keypad_button(CLAY_STRING("Up"), model->key_state & InputKeyUp);
                keypad_test_app_create_empty();
                keypad_test_app_create_empty();
            }
            CLAY_AUTO_ID({.layout = layout_row}) {
                keypad_test_app_create_keypad_button(CLAY_STRING("Left"), model->key_state & InputKeyLeft);
                keypad_test_app_create_keypad_button(CLAY_STRING("Ok"), model->key_state & InputKeyOk);
                keypad_test_app_create_keypad_button(CLAY_STRING("Right"), model->key_state & InputKeyRight);
            }
            CLAY_AUTO_ID({.layout = layout_row}) {
                keypad_test_app_create_keypad_button(CLAY_STRING("Down"), model->key_state & InputKeyDown);
            }
            CLAY_AUTO_ID({.layout = layout_row}) {
                keypad_test_app_create_keypad_button(CLAY_STRING("SW"), model->key_state & InputKeySw);
                keypad_test_app_create_empty();
                keypad_test_app_create_empty();
                keypad_test_app_create_empty();
                if(model->exit_counter > 0) {
                    keypad_test_app_create_keypad_button(clay_helper_string_from(model->exit_text), model->key_state & InputKeyBack);
                } else {
                    keypad_test_app_create_keypad_button(CLAY_STRING("Back"), model->key_state & InputKeyBack);
                }
            }
            CLAY_AUTO_ID({.layout = layout_row}) {
                keypad_test_app_create_keypad_button(CLAY_STRING("1"), model->key_state & InputKey1);
                keypad_test_app_create_keypad_button(CLAY_STRING("2"), model->key_state & InputKey2);
                keypad_test_app_create_keypad_button(CLAY_STRING("P"), model->key_state & InputKey3);
                keypad_test_app_create_keypad_button(CLAY_STRING("4"), model->key_state & InputKey4);
                keypad_test_app_create_keypad_button(CLAY_STRING("5"), model->key_state & InputKey5);
            }
        }
    }
}

static bool keypad_test_app_input_logic(InputEvent* event, void* context) {
    furi_check(context);
    KeypadTestApp* instance = context;
    bool consumed = false;

    if(event->type == InputTypePress || event->type == InputTypeRelease) {
        with_view_model(
            instance->view_port,
            KeypadTestModel * model,
            {
                if(event->type == InputTypePress) {
                    model->key_state |= event->key;
                } else {
                    model->key_state &= ~event->key;
                }
            },
            true);
    }

    if(event->key == InputKeyBack) {
        if(event->type == InputTypePress) {
            with_view_model(
                instance->view_port,
                KeypadTestModel * model,
                {
                    model->exit_counter = 1;
                    furi_string_printf(model->exit_text, "%zu", KEYPAD_TEST_TICKS_TO_EXIT);
                },
                true);
            view_port_update(instance->view_port);
        } else if(event->type == InputTypeRelease) {
            with_view_model(
                instance->view_port,
                KeypadTestModel * model,
                {
                    model->exit_counter = 0;
                    furi_string_set(model->exit_text, "");
                },
                true);
            view_port_update(instance->view_port);
        } else if(event->type == InputTypeRepeat) {
            with_view_model(
                instance->view_port,
                KeypadTestModel * model,
                {
                    model->exit_counter++;
                    furi_string_printf(model->exit_text, "%zu", KEYPAD_TEST_TICKS_TO_EXIT - model->exit_counter);

                    if(model->exit_counter >= KEYPAD_TEST_TICKS_TO_EXIT) {
                        furi_thread_signal(instance->thread, FuriSignalExit, NULL);
                    }
                },
                true);
        }
    }

    return consumed;
}

static KeypadTestApp* keypad_test_app_alloc(void) {
    KeypadTestApp* instance = malloc(sizeof(KeypadTestApp));
    instance->gui = furi_record_open(RECORD_GUI);
    instance->event_loop = furi_event_loop_alloc();
    instance->thread = furi_thread_get_current();

    instance->view_port = view_port_alloc();
    view_port_allocate_model(instance->view_port, ViewPortModelTypeLockFree, sizeof(KeypadTestModel));
    with_view_model(
        instance->view_port,
        KeypadTestModel * model,
        {
            model->key_state = 0;
            model->exit_counter = 0;
            model->exit_text = furi_string_alloc();
        },
        false);
    view_port_set_layout_callback(instance->view_port, keypad_test_app_layout);
    view_port_set_input_callback(instance->view_port, keypad_test_app_input_logic, instance);
    gui_add_view_port(instance->gui, instance->view_port, GuiViewPriorityApplication);
    return instance;
}

static void keypad_test_app_free(KeypadTestApp* instance) {
    gui_remove_view_port(instance->gui, instance->view_port);
    furi_record_close(RECORD_GUI);
    with_view_model(instance->view_port, KeypadTestModel * model, { furi_string_free(model->exit_text); }, false);
    view_port_free(instance->view_port);
    furi_event_loop_free(instance->event_loop);
    free(instance);
}

int32_t keypad_test_app(void* p) {
    KeypadTestApp* instance = keypad_test_app_alloc();
    furi_event_loop_run(instance->event_loop);
    keypad_test_app_free(instance);
    return 0;
}
