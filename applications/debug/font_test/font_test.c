#include <furi.h>
#include <gui/gui.h>
#include <gui/clay_helper.h>

#define TAG "FontTest"

/* Printable ASCII split into rows of 16 glyphs (the last row has 15). */
static const Clay_String font_test_rows[] = {
    CLAY_STRING(" !\"#$%&'()*+,-./"),
    CLAY_STRING("0123456789:;<=>?"),
    CLAY_STRING("@ABCDEFGHIJKLMNO"),
    CLAY_STRING("PQRSTUVWXYZ[\\]^_"),
    CLAY_STRING("`abcdefghijklmno"),
    CLAY_STRING("pqrstuvwxyz{|}~"),
};

static const char* font_test_names[] = {
    [FontBody] = "FontBody     haxrcorp4089",
    [FontButton] = "FontButton   helvB08",
    [FontKeyboard] = "FontKeyboard profont11",
    [FontBusy9] = "FontBusy9    busy9_9px",
};

typedef struct {
    uint32_t font_index;
} FontTestModel;

typedef struct {
    Gui* gui;
    View* view;
    FuriEventLoop* event_loop;
    //FuriThread* thread;
} FontTest;

static bool font_test_layout(void* _model) {
    furi_assert(_model);
    FontTestModel* model = (FontTestModel*)_model;
    Font font = (Font)model->font_index;

    CLAY(
        CLAY_APP_ID("Root"),
        {.backgroundColor = COLOR_WHITE,
         .layout = {
             .layoutDirection = CLAY_TOP_TO_BOTTOM,
             .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)},
             .padding = {4, 4, 4, 4},
             .childGap = 2,
         }}) {
        /* Title: font name, centred */
        CLAY_AUTO_ID({
            .layout =
                {
                    .sizing = {.width = CLAY_SIZING_GROW(0)},
                    .childAlignment = {.x = CLAY_ALIGN_X_LEFT},
                },
        }) {
            CLAY_TEXT(clay_helper_string_from_chars(font_test_names[font]), CLAY_TEXT_CONFIG({.fontId = FontButton, .textColor = COLOR_BLACK}));
        }

        /* Separator */
        CLAY_AUTO_ID({
            .backgroundColor = COLOR_BLACK,
            .layout = {.sizing = {.height = CLAY_SIZING_FIXED(1), .width = CLAY_SIZING_GROW(0)}},
        }) {
        }

        /* Glyph rows rendered with the selected font */
        for(size_t i = 0; i < COUNT_OF(font_test_rows); i++) {
            CLAY_AUTO_ID({.layout = {.sizing = {.width = CLAY_SIZING_GROW(0)}}}) {
                CLAY_TEXT(font_test_rows[i], CLAY_TEXT_CONFIG({.fontId = font, .textColor = COLOR_BLACK}));
            }
        }

        /* Footer: navigation hint */
        CLAY_AUTO_ID({
            .layout =
                {
                    .layoutDirection = CLAY_LEFT_TO_RIGHT,
                    .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(1)},
                    .childAlignment = {.y = CLAY_ALIGN_Y_BOTTOM},
                    .childGap = 4,
                },
        }) {
            CLAY_TEXT(CLAY_STRING("<  >  switch"), CLAY_TEXT_CONFIG({.fontId = FontBusy9, .textColor = COLOR_BLACK}));
            CLAY_AUTO_ID({.layout = {.sizing = {.width = CLAY_SIZING_GROW(1)}}}) {
            }
            CLAY_TEXT(CLAY_STRING("Back exit"), CLAY_TEXT_CONFIG({.fontId = FontBusy9, .textColor = COLOR_BLACK}));
        }
    }

    return false;
}

static bool font_test_input(InputEvent* event, void* context) {
    furi_check(context);
    FontTest* instance = context;
    bool consumed = false;

    if(event->type == InputTypePress) {
        if(event->key == InputKeyBack) {
            furi_event_loop_stop(instance->event_loop);
            consumed = true;
        }

        if(event->key == InputKeyLeft) {
            with_view_model(instance->view, FontTestModel * model, { model->font_index = (model->font_index + FontMax - 1) % FontMax; }, true);
            consumed = true;
        }

        if(event->key == InputKeyRight) {
            with_view_model(instance->view, FontTestModel * model, { model->font_index = (model->font_index + 1) % FontMax; }, true);
            consumed = true;
        }
    }
    return consumed;
}

static FontTest* font_test_alloc(void) {
    FontTest* instance = malloc(sizeof(FontTest));
    instance->gui = furi_record_open(RECORD_GUI);
    instance->event_loop = furi_event_loop_alloc();
   // instance->thread = furi_thread_get_current();
    instance->view = view_alloc();
    view_allocate_model(instance->view, ViewModelTypeLockFree, sizeof(FontTestModel));
    with_view_model(instance->view, FontTestModel * model, { model->font_index = FontBusy9; }, false);

    view_set_layout_callback(instance->view, font_test_layout);
    view_set_input_callback(instance->view, font_test_input, instance);
    gui_add_view(instance->gui, instance->view, GuiViewPriorityApplication);
    return instance;
}

static void font_test_free(FontTest* instance) {
    gui_remove_view(instance->gui, instance->view);
    furi_record_close(RECORD_GUI);
    view_free(instance->view);
    furi_event_loop_free(instance->event_loop);
    free(instance);
}

int32_t font_test_app(void* p) {
    UNUSED(p);
    FontTest* instance = font_test_alloc();
    furi_event_loop_run(instance->event_loop);
    font_test_free(instance);
    return 0;
}
