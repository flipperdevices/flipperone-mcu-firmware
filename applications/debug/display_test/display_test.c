#include <furi.h>
#include <gui/gui.h>
#include <gui/clay_helper.h>

#define TAG "DisplayTest"

#define DISPLAY_TEST_MESSAGE_QUEUE_SIZE (8)

// Diagonal lines: 45 degrees, DIAGONAL_WIDTH lit pixels followed by DIAGONAL_WIDTH dark pixels along each row
#define DISPLAY_TEST_DIAGONAL_WIDTH (2)

#define DISPLAY_TEST_SQUARE_SIZE    (20)
#define DISPLAY_TEST_SQUARE_SPEED_X (180) // px/s
#define DISPLAY_TEST_SQUARE_SPEED_Y (120) // px/s
#define DISPLAY_TEST_FIXED_SHIFT    (8) // fractional bits of the square position

#define DISPLAY_TEST_FPS_WINDOW_MS (1000)
#define DISPLAY_TEST_FPS_UNLIMITED (UINT32_MAX) // render back to back as fast as the GUI can

#define DISPLAY_TEST_COLOR_ON  (0xFF)
#define DISPLAY_TEST_COLOR_OFF (0x00)

typedef enum {
    DisplayTestPatternBlack,
    DisplayTestPatternWhite,
    DisplayTestPatternVerticalEven,
    DisplayTestPatternVerticalOdd,
    DisplayTestPatternHorizontalEven,
    DisplayTestPatternHorizontalOdd,
    DisplayTestPatternDiagonalRight,
    DisplayTestPatternDiagonalLeft,
    DisplayTestPatternBounce15,
    DisplayTestPatternBounce30,
    DisplayTestPatternBounce60,
    DisplayTestPatternBounceUnlimited,
    DisplayTestPatternBounceInverted15,
    DisplayTestPatternBounceInverted30,
    DisplayTestPatternBounceInverted60,
    DisplayTestPatternBounceInvertedUnlimited,
    DisplayTestPatternCount,
} DisplayTestPattern;

typedef enum {
    DisplayTestSequenceLeftRight,
    DisplayTestSequencePtt,
} DisplayTestSequence;

typedef struct {
    DisplayTestSequence sequence;
    int32_t delta;
} DisplayTestInput;

typedef struct {
    Canvas* canvas;
    Image image;

    DisplayTestPattern pattern;

    // Independent cursor per input sequence, so each one keeps its own position
    size_t pattern_index_left_right;
    size_t pattern_index_ptt;

    bool dirty;

    // Square position in fixed point, velocity in fixed point px per frame
    // (unlimited mode recomputes velocity every frame from the elapsed time)
    int32_t square_x;
    int32_t square_y;
    int32_t square_dx;
    int32_t square_dy;
    uint32_t square_tick;

    // Measured frame rate of the dynamic pattern
    uint32_t fps_frames;
    uint32_t fps_window_start;
    uint32_t fps_measured;
    char fps_text[24];
} DisplayTestModel;

typedef struct {
    Gui* gui;
    View* view;
    FuriEventLoop* event_loop;
    FuriEventLoopTimer* frame_timer;
    FuriMessageQueue* input_queue;
    FuriThread* thread;

    // Frame scheduling, app thread only
    uint32_t frame_fps;
    uint32_t frame_start;
    uint32_t frame_index;
} DisplayTest;

static uint32_t display_test_pattern_fps(DisplayTestPattern pattern) {
    switch(pattern) {
    case DisplayTestPatternBounce15:
    case DisplayTestPatternBounceInverted15:
        return 15;
    case DisplayTestPatternBounce30:
    case DisplayTestPatternBounceInverted30:
        return 30;
    case DisplayTestPatternBounce60:
    case DisplayTestPatternBounceInverted60:
        return 60;
    case DisplayTestPatternBounceUnlimited:
    case DisplayTestPatternBounceInvertedUnlimited:
        return DISPLAY_TEST_FPS_UNLIMITED;
    default:
        return 0;
    }
}

// Black square on white background instead of white on black
static bool display_test_pattern_inverted(DisplayTestPattern pattern) {
    return pattern >= DisplayTestPatternBounceInverted15 && pattern <= DisplayTestPatternBounceInvertedUnlimited;
}

// Patterns reachable with the left/right keys
static const DisplayTestPattern display_test_patterns_left_right[] = {
    DisplayTestPatternBlack,
    DisplayTestPatternWhite,
    DisplayTestPatternVerticalEven,
    DisplayTestPatternVerticalOdd,
    DisplayTestPatternHorizontalEven,
    DisplayTestPatternHorizontalOdd,
    DisplayTestPatternDiagonalRight,
    DisplayTestPatternDiagonalLeft,
    DisplayTestPatternBounce15,
    DisplayTestPatternBounce30,
    DisplayTestPatternBounce60,
    DisplayTestPatternBounceUnlimited,
    DisplayTestPatternBounceInverted15,
    DisplayTestPatternBounceInverted30,
    DisplayTestPatternBounceInverted60,
    DisplayTestPatternBounceInvertedUnlimited,
};

// Patterns reachable with the PTT key
static const DisplayTestPattern display_test_patterns_ptt[] = {
    DisplayTestPatternBlack,
    DisplayTestPatternWhite,
    DisplayTestPatternVerticalEven,
    DisplayTestPatternVerticalOdd,
    DisplayTestPatternHorizontalEven,
    DisplayTestPatternHorizontalOdd,
    DisplayTestPatternDiagonalRight,
    DisplayTestPatternDiagonalLeft,
    DisplayTestPatternBounce60,
    DisplayTestPatternBounceInverted60,
};

static void display_test_draw_static(DisplayTestModel* model) {
    Color* data = canvas_get_data(model->canvas);
    const int32_t width = canvas_get_width(model->canvas);
    const int32_t height = canvas_get_height(model->canvas);

    for(int32_t y = 0; y < height; y++) {
        Color* row = &data[y * width];
        for(int32_t x = 0; x < width; x++) {
            bool on = false;
            switch(model->pattern) {
            case DisplayTestPatternWhite:
                on = true;
                break;
            case DisplayTestPatternVerticalEven:
                on = (x % 2) == 0;
                break;
            case DisplayTestPatternVerticalOdd:
                on = (x % 2) == 1;
                break;
            case DisplayTestPatternHorizontalEven:
                on = (y % 2) == 0;
                break;
            case DisplayTestPatternHorizontalOdd:
                on = (y % 2) == 1;
                break;
            case DisplayTestPatternDiagonalRight:
                // "/" lines: pixel moves right when going up
                on = ((x + y) % (2 * DISPLAY_TEST_DIAGONAL_WIDTH)) < DISPLAY_TEST_DIAGONAL_WIDTH;
                break;
            case DisplayTestPatternDiagonalLeft:
                // "\" lines: pixel moves left when going up; offset keeps the operand positive
                on = ((x - y + height * 2 * DISPLAY_TEST_DIAGONAL_WIDTH) % (2 * DISPLAY_TEST_DIAGONAL_WIDTH)) < DISPLAY_TEST_DIAGONAL_WIDTH;
                break;
            default:
                break;
            }
            row[x] = on ? DISPLAY_TEST_COLOR_ON : DISPLAY_TEST_COLOR_OFF;
        }
    }
}

static void display_test_draw_bounce(DisplayTestModel* model) {
    Color* data = canvas_get_data(model->canvas);
    const int32_t width = canvas_get_width(model->canvas);
    const bool inverted = display_test_pattern_inverted(model->pattern);
    const Color background = inverted ? DISPLAY_TEST_COLOR_ON : DISPLAY_TEST_COLOR_OFF;
    const Color square = inverted ? DISPLAY_TEST_COLOR_OFF : DISPLAY_TEST_COLOR_ON;

    canvas_clear(model->canvas, background);

    const int32_t x0 = model->square_x >> DISPLAY_TEST_FIXED_SHIFT;
    const int32_t y0 = model->square_y >> DISPLAY_TEST_FIXED_SHIFT;
    for(int32_t y = y0; y < y0 + DISPLAY_TEST_SQUARE_SIZE; y++) {
        memset(&data[y * width + x0], square, DISPLAY_TEST_SQUARE_SIZE);
    }
}

static void display_test_fps_update(DisplayTestModel* model) {
    const uint32_t now = furi_get_tick();
    model->fps_frames++;

    const uint32_t elapsed = now - model->fps_window_start;
    if(elapsed >= furi_ms_to_ticks(DISPLAY_TEST_FPS_WINDOW_MS)) {
        model->fps_measured = (model->fps_frames * furi_kernel_get_tick_frequency() + elapsed / 2) / elapsed;
        model->fps_frames = 0;
        model->fps_window_start = now;
    }

    const uint32_t fps = display_test_pattern_fps(model->pattern);
    if(fps == DISPLAY_TEST_FPS_UNLIMITED) {
        snprintf(model->fps_text, sizeof(model->fps_text), "%lu FPS (max)", (unsigned long)model->fps_measured);
    } else {
        snprintf(model->fps_text, sizeof(model->fps_text), "%lu FPS (%lu)", (unsigned long)model->fps_measured, (unsigned long)fps);
    }
}

// Velocity for a frame that lasts ticks / tick_frequency seconds, keeps the current direction
static void display_test_square_set_velocity(DisplayTestModel* model, uint32_t ticks, uint32_t tick_frequency) {
    const int32_t dx = (int32_t)(((uint32_t)DISPLAY_TEST_SQUARE_SPEED_X << DISPLAY_TEST_FIXED_SHIFT) * ticks / tick_frequency);
    const int32_t dy = (int32_t)(((uint32_t)DISPLAY_TEST_SQUARE_SPEED_Y << DISPLAY_TEST_FIXED_SHIFT) * ticks / tick_frequency);

    model->square_dx = (model->square_dx < 0) ? -dx : dx;
    model->square_dy = (model->square_dy < 0) ? -dy : dy;
}

static void display_test_square_step(DisplayTestModel* model) {
    const int32_t max_x = (canvas_get_width(model->canvas) - DISPLAY_TEST_SQUARE_SIZE) << DISPLAY_TEST_FIXED_SHIFT;
    const int32_t max_y = (canvas_get_height(model->canvas) - DISPLAY_TEST_SQUARE_SIZE) << DISPLAY_TEST_FIXED_SHIFT;

    model->square_x += model->square_dx;
    if(model->square_x < 0) {
        model->square_x = -model->square_x;
        model->square_dx = -model->square_dx;
    } else if(model->square_x > max_x) {
        model->square_x = 2 * max_x - model->square_x;
        model->square_dx = -model->square_dx;
    }

    model->square_y += model->square_dy;
    if(model->square_y < 0) {
        model->square_y = -model->square_y;
        model->square_dy = -model->square_dy;
    } else if(model->square_y > max_y) {
        model->square_y = 2 * max_y - model->square_y;
        model->square_dy = -model->square_dy;
    }
}

// Unlimited mode: no timer, the square moves by the time elapsed since the previous frame
static void display_test_square_step_elapsed(DisplayTestModel* model) {
    const uint32_t now = furi_get_tick();
    const uint32_t elapsed = now - model->square_tick;

    // Sub-tick frames are accumulated until at least one tick passes
    if(elapsed) {
        model->square_tick = now;
        // Clamp long stalls so a single step can not overshoot the bounce reflection
        const uint32_t ticks = MIN(elapsed, furi_ms_to_ticks(100));
        display_test_square_set_velocity(model, ticks, furi_kernel_get_tick_frequency());
        display_test_square_step(model);
    }
}

// Called from GUI thread: all canvas pixel writes must happen here, see Clay image render notes
static bool display_test_layout(void* _model) {
    furi_assert(_model);
    DisplayTestModel* model = (DisplayTestModel*)_model;
    const uint32_t fps = display_test_pattern_fps(model->pattern);
    const bool dynamic = fps != 0;
    const bool unlimited = fps == DISPLAY_TEST_FPS_UNLIMITED;
    const bool inverted = display_test_pattern_inverted(model->pattern);

    if(unlimited) {
        display_test_square_step_elapsed(model);
        model->dirty = true;
    }

    if(model->dirty) {
        model->dirty = false;
        if(dynamic) {
            if(!unlimited) {
                // One step per drawn frame: timer ticks that land while a frame is still pending
                // only re-set the flag, so the square never jumps over an undrawn position
                display_test_square_step(model);
            }
            display_test_draw_bounce(model);
            display_test_fps_update(model);
        } else {
            display_test_draw_static(model);
        }
    }

    CLAY(
        CLAY_APP_ID("Pattern"),
        {
            .layout = {.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)}},
            .image = {.imageData = &model->image},
        }) {
        if(dynamic) {
            CLAY(
                CLAY_APP_ID("Fps"),
                {
                    .backgroundColor = inverted ? COLOR_WHITE : COLOR_BLACK,
                    .layout = {.padding = {4, 4, 2, 2}},
                    .floating =
                        {
                            .attachPoints = {.element = CLAY_ATTACH_POINT_RIGHT_TOP, .parent = CLAY_ATTACH_POINT_RIGHT_TOP},
                            .attachTo = CLAY_ATTACH_TO_PARENT,
                        },
                }) {
                CLAY_TEXT(
                    clay_helper_string_from_chars(model->fps_text), CLAY_TEXT_CONFIG({.fontId = FontBody, .textColor = inverted ? COLOR_BLACK : COLOR_WHITE}));
            }
        }
    }

    // Unlimited mode: request the next frame right away
    return unlimited;
}

static void display_test_square_reset(DisplayTestModel* model, uint32_t fps) {
    const int32_t width = canvas_get_width(model->canvas);
    const int32_t height = canvas_get_height(model->canvas);

    // Keep the speed in px/s constant so the motion looks the same at every frame rate
    if(fps != DISPLAY_TEST_FPS_UNLIMITED) {
        display_test_square_set_velocity(model, 1, fps);
    }
    model->square_tick = furi_get_tick();
    model->square_x = MIN(model->square_x, (width - DISPLAY_TEST_SQUARE_SIZE) << DISPLAY_TEST_FIXED_SHIFT);
    model->square_y = MIN(model->square_y, (height - DISPLAY_TEST_SQUARE_SIZE) << DISPLAY_TEST_FIXED_SHIFT);
}

// Schedules the next frame against an absolute deadline so 1000/fps rounding does not drift
static void display_test_frame_schedule(DisplayTest* instance) {
    const uint32_t tick_frequency = furi_kernel_get_tick_frequency();
    const uint32_t now = furi_get_tick();

    instance->frame_index++;
    uint32_t deadline = instance->frame_start + (instance->frame_index * tick_frequency) / instance->frame_fps;
    int32_t interval = (int32_t)(deadline - now);

    if(interval <= 0) {
        // Fell behind by more than a frame: resync instead of bursting to catch up
        instance->frame_start = now;
        instance->frame_index = 0;
        interval = 1;
    }

    furi_event_loop_timer_start(instance->frame_timer, interval);
}

static void display_test_frame_timer_callback(void* context) {
    furi_check(context);
    DisplayTest* instance = context;

    with_view_model(instance->view, DisplayTestModel * model, { model->dirty = true; }, true);

    display_test_frame_schedule(instance);
}

static void display_test_pattern_switch(DisplayTest* instance, DisplayTestInput input) {
    uint32_t fps = 0;

    with_view_model(
        instance->view,
        DisplayTestModel * model,
        {
            const DisplayTestPattern* patterns;
            size_t patterns_count;
            size_t* index;

            if(input.sequence == DisplayTestSequencePtt) {
                patterns = display_test_patterns_ptt;
                patterns_count = COUNT_OF(display_test_patterns_ptt);
                index = &model->pattern_index_ptt;
            } else {
                patterns = display_test_patterns_left_right;
                patterns_count = COUNT_OF(display_test_patterns_left_right);
                index = &model->pattern_index_left_right;
            }

            if(input.delta > 0) {
                *index = (*index + 1) % patterns_count;
            } else {
                *index = (*index + patterns_count - 1) % patterns_count;
            }

            model->pattern = patterns[*index];
            model->dirty = true;
            fps = display_test_pattern_fps(model->pattern);
            if(fps) {
                display_test_square_reset(model, fps);
                model->fps_frames = 0;
                model->fps_window_start = furi_get_tick();
                model->fps_measured = 0;
            }
            FURI_LOG_I(TAG, "Pattern %d", model->pattern);
        },
        true);

    furi_event_loop_timer_stop(instance->frame_timer);
    if(fps && fps != DISPLAY_TEST_FPS_UNLIMITED) {
        instance->frame_fps = fps;
        instance->frame_start = furi_get_tick();
        instance->frame_index = 0;
        display_test_frame_schedule(instance);
    }
}

static void display_test_input_queue_callback(FuriEventLoopObject* object, void* context) {
    furi_check(context);
    DisplayTest* instance = context;
    DisplayTestInput input;

    while(furi_message_queue_get(object, &input, 0) == FuriStatusOk) {
        display_test_pattern_switch(instance, input);
    }
}

static bool display_test_input(InputEvent* event, void* context) {
    furi_check(context);
    DisplayTest* instance = context;
    bool consumed = false;

    if(event->key == InputKeyBack) {
        if(event->type == InputTypePress) {
            furi_thread_signal(instance->thread, FuriSignalExit, NULL);
            consumed = true;
        }
    } else if(event->type == InputTypePress || event->type == InputTypeRepeat) {
        // Timer belongs to the app thread, so pattern switching is forwarded there
        DisplayTestInput input = {.sequence = DisplayTestSequenceLeftRight, .delta = 0};
        if(event->key == InputKeyRight) {
            input.delta = 1;
        } else if(event->key == InputKeyLeft) {
            input.delta = -1;
        } else if(event->key == InputKeyPtt && event->type == InputTypePress) {
            // PTT advances its own sequence and does not repeat while held
            input.sequence = DisplayTestSequencePtt;
            input.delta = 1;
        }

        if(input.delta) {
            furi_message_queue_put(instance->input_queue, &input, 0);
            consumed = true;
        }
    }

    return consumed;
}

static DisplayTest* display_test_alloc(void) {
    DisplayTest* instance = malloc(sizeof(DisplayTest));
    instance->gui = furi_record_open(RECORD_GUI);
    instance->event_loop = furi_event_loop_alloc();
    instance->thread = furi_thread_get_current();

    instance->frame_timer = furi_event_loop_timer_alloc(instance->event_loop, display_test_frame_timer_callback, FuriEventLoopTimerTypeOnce, instance);
    instance->input_queue = furi_message_queue_alloc(DISPLAY_TEST_MESSAGE_QUEUE_SIZE, sizeof(DisplayTestInput));
    furi_event_loop_subscribe_message_queue(instance->event_loop, instance->input_queue, FuriEventLoopEventIn, display_test_input_queue_callback, instance);

    instance->view = view_alloc();
    view_allocate_model(instance->view, ViewModelTypeLocking, sizeof(DisplayTestModel));

    const size_t width = gui_get_width(instance->gui);
    const size_t height = gui_get_height(instance->gui);
    with_view_model(
        instance->view,
        DisplayTestModel * model,
        {
            model->canvas = canvas_alloc(width, height);
            model->image = canvas_to_image(model->canvas);
            model->pattern = DisplayTestPatternBlack;
            // Both sequences start on Black, which is their first entry
            model->pattern_index_left_right = 0;
            model->pattern_index_ptt = 0;
            model->dirty = true;
            model->square_dx = 1;
            model->square_dy = 1;
        },
        false);

    view_set_layout_callback(instance->view, display_test_layout);
    view_set_input_callback(instance->view, display_test_input, instance);
    gui_add_view(instance->gui, instance->view, GuiViewPriorityApplication);
    return instance;
}

static void display_test_free(DisplayTest* instance) {
    gui_remove_view(instance->gui, instance->view);
    furi_record_close(RECORD_GUI);

    furi_event_loop_timer_free(instance->frame_timer);
    furi_event_loop_unsubscribe(instance->event_loop, instance->input_queue);
    furi_message_queue_free(instance->input_queue);

    with_view_model(instance->view, DisplayTestModel * model, { canvas_free(model->canvas); }, false);
    view_free(instance->view);
    furi_event_loop_free(instance->event_loop);
    free(instance);
}

int32_t display_test_app(void* p) {
    UNUSED(p);
    DisplayTest* instance = display_test_alloc();
    furi_event_loop_run(instance->event_loop);
    display_test_free(instance);
    return 0;
}
