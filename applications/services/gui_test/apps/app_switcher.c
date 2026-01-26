#include "apps.h"
#include <drivers/display/display_jd9853_reg.h>

#define TAG "Switcher"
const float app_switch_speed = 0.333f;

typedef struct {
    RenderBuffer** apps_buffers;
    Image** images;
    int* heights;
    uint8_t* tints;

    float current_app_index;

    bool touch_active;
    float touch_last_x;
    float touch_last_y;

    int app_index;

    App* const* apps;
    size_t app_count;
} SwitcherState;

static void switcher_calculate_apps_state(float selected_app_index_animated, int* heights, uint8_t* tints, size_t app_count) {
    const int TOTAL_HEIGHT = JD9853_HEIGHT;
    const int HEIGHT_SELECTED = 102;
    const int HEIGHT_ADJACENT = 21;
    const uint8_t tinted = 75;
    const uint8_t normal = 0;

    float frac = selected_app_index_animated - floorf(selected_app_index_animated);
    int base_index = (int)floorf(selected_app_index_animated);

    // Indexes for state A (base_index selected)
    int prev_a = base_index - 1;
    int sel_a = base_index;
    int next_a = base_index + 1;

    // Indexes for state B (base_index + 1 selected)
    int prev_b = base_index;
    int sel_b = base_index + 1;
    int next_b = base_index + 2;

    // Linear interpolation for each app
    for(size_t i = 0; i < app_count; i++) {
        int h_a = 0;
        int h_b = 0;
        uint8_t tint_a = normal;
        uint8_t tint_b = normal;

        // Height in state A
        if((int)i == prev_a || (int)i == next_a) {
            h_a = HEIGHT_ADJACENT;
            tint_a = tinted;
        }
        if((int)i == sel_a) {
            h_a = HEIGHT_SELECTED;
            tint_a = normal;
        }

        // Height in state B
        if((int)i == prev_b || (int)i == next_b) {
            h_b = HEIGHT_ADJACENT;
            tint_b = tinted;
        }
        if((int)i == sel_b) {
            h_b = HEIGHT_SELECTED;
            tint_b = normal;
        }

        heights[i] = (int)(h_a * (1.0f - frac) + h_b * frac);
        tints[i] = (uint8_t)(tint_a * (1.0f - frac) + tint_b * frac);
    }

    // Correct rounding error
    int sum = 0;
    for(size_t i = 0; i < app_count; i++) {
        sum += heights[i];
    }
    int diff = TOTAL_HEIGHT - sum;

    // Add difference to the closest to the selected app
    int main_app = (int)roundf(selected_app_index_animated);
    if(main_app >= 0 && (size_t)main_app < app_count) {
        heights[main_app] += diff;
    }
}

static void switcher_apps_layout(Image** images, size_t* heights, uint8_t* tints, size_t apps_count) {
    Clay_Sizing layoutExpand = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};

    CLAY(
        CLAY_APP_ID("OuterContainer"),
        {.backgroundColor = COLOR_WHITE,
         .layout = {
             .layoutDirection = CLAY_TOP_TO_BOTTOM,
             .sizing = layoutExpand,
             .padding = {0},
             .childGap = 4,
         }}) {
        CLAY_AUTO_ID({
            .layout =
                {
                    .layoutDirection = CLAY_TOP_TO_BOTTOM,
                    .childGap = 0,
                    .sizing = layoutExpand,
                    .childAlignment =
                        {
                            .x = CLAY_ALIGN_X_CENTER,
                            .y = CLAY_ALIGN_Y_BOTTOM,
                        },
                },
            .backgroundColor = COLOR_BLACK,
        }) {
            for(size_t i = 0; i < apps_count; i++) {
                CLAY_AUTO_ID({
                    .layout = {.sizing = {.width = JD9853_WIDTH, .height = heights[i]}},
                    .border = {.color = COLOR_BLACK, .width = {.top = 1, .left = 0, .right = 0, .bottom = 0}},
                    .image = {.imageData = images[i]},
                    .backgroundColor = {tints[i], tints[i], tints[i], 255},
                }) {
                }
            }
        }
    }
}

static void switcher_layout(App* app) {
    furi_assert(app);
    SwitcherState* state = (SwitcherState*)app->state;

    switcher_calculate_apps_state(state->current_app_index, state->heights, state->tints, state->app_count);

    switcher_apps_layout(state->images, state->heights, state->tints, state->app_count);

    if(!state->touch_active) {
        if(state->current_app_index < state->app_index) {
            state->current_app_index += app_switch_speed;
            if(state->current_app_index > state->app_index) {
                state->current_app_index = state->app_index;
            }
        } else if(state->current_app_index > state->app_index) {
            state->current_app_index -= app_switch_speed;
            if(state->current_app_index < state->app_index) {
                state->current_app_index = state->app_index;
            }
        }
    }
}

static bool switcher_input(App* app, const GuiTestMessage* message) {
    furi_assert(app);
    furi_assert(message);

    SwitcherState* state = (SwitcherState*)app->state;
    bool handled = false;
    switch(message->type) {
    case GuiTestMessageTypeInputTouchEvent: {
        InputTouchEvent event = message->input_touch_event;
        if(event.type == InputTouchTypeStart) {
            state->touch_active = true;
            state->touch_last_x = event.x;
            state->touch_last_y = event.y;
        } else if(event.type == InputTouchTypeEnd) {
            state->touch_active = false;
            state->app_index = (int)roundf(state->current_app_index);
        } else if(event.type == InputTouchTypeMove) {
            if(state->touch_active) {
                float delta_y = (float)(event.y - state->touch_last_y);
                float sensitivity = 0.008f;
                state->current_app_index += delta_y * sensitivity;
                if(state->current_app_index < 0.0f) state->current_app_index = 0.0f;
                if(state->current_app_index > (float)(state->app_count - 1)) state->current_app_index = (float)(state->app_count - 1);
                state->touch_last_x = event.x;
                state->touch_last_y = event.y;
            }
        }
        handled = true;
    } break;
    case GuiTestMessageTypeInputEvent: {
        InputEvent event = message->input_event;
        if(event.type == InputTypePress) {
            if(event.key == InputKeyUp) {
                if(state->app_index > 0) state->app_index -= 1;
            } else if(event.key == InputKeyDown) {
                if(state->app_index < (state->app_count - 1)) state->app_index += 1;
            }
        }
    } break;
    }
    return handled;
}

const App app_switcher = {
    .state = &(SwitcherState){0},
    .input = switcher_input,
    .render = switcher_layout,
};

void app_switcher_set_app_index(int index) {
    SwitcherState* state = (SwitcherState*)app_switcher.state;
    if(index < 0) index = 0;
    if(index >= (int)state->app_count) index = (int)state->app_count - 1;
    state->app_index = index;
    state->current_app_index = (float)index;

    RenderBuffer* current_buffer = render_get_current_buffer();
    render_set_current_buffer(state->apps_buffers[state->app_index]);

    Clay_ResetMeasureTextCache();
    Clay_BeginLayout();
    apps_call_render(state->apps[state->app_index]);
    Clay_RenderCommandArray renderCommands = Clay_EndLayout();
    render_clear_buffer(0x00);
    render_do_render(&renderCommands);

    render_set_current_buffer(current_buffer);
}

int app_switcher_get_app_index() {
    SwitcherState* state = (SwitcherState*)app_switcher.state;
    return state->app_index;
}

void app_switcher_init(App* app, App* const _apps[], size_t _app_count) {
    furi_assert(app);
    SwitcherState* state = (SwitcherState*)app->state;

    if(!state->apps_buffers) {
        RenderBuffer* current_buffer = render_get_current_buffer();
        state->apps = _apps;
        state->app_count = _app_count;

        state->apps_buffers = malloc(sizeof(RenderBuffer*) * state->app_count);
        for(int i = 0; i < state->app_count; i++) {
            state->apps_buffers[i] = render_alloc_buffer();
            render_set_current_buffer(state->apps_buffers[i]);

            Clay_ResetMeasureTextCache();
            Clay_BeginLayout();
            apps_call_render(state->apps[i]);
            Clay_RenderCommandArray renderCommands = Clay_EndLayout();
            render_clear_buffer(0x00);
            render_do_render(&renderCommands);
        }
        render_set_current_buffer(current_buffer);

        state->heights = malloc(sizeof(int) * state->app_count);
        state->tints = malloc(sizeof(uint8_t) * state->app_count);
        state->images = malloc(sizeof(Image*) * state->app_count);

        for(int i = 0; i < state->app_count; i++) {
            state->images[i] = malloc(sizeof(Image));
            state->images[i]->format = ImageFormatRawGray8;
            state->images[i]->width = render_get_buffer_width(state->apps_buffers[i]);
            state->images[i]->height = render_get_buffer_height(state->apps_buffers[i]);
            state->images[i]->data = render_get_buffer_data(state->apps_buffers[i]);
        }

        state->current_app_index = state->app_index;
        state->touch_active = false;
        state->touch_last_x = 0;
        state->touch_last_y = 0;
    }
}
