#include "parallax.h"
#include <furi.h>
#include <math.h>

#define TAG "Parallax"

/* The scene is redrawn at this rate while anything in it moves. */
#define PARALLAX_FRAME_PERIOD_MS 33

/* A pass that comes this long after the previous one (the scene was hidden)
 * advances the offsets by this much at most, so a stale clock is not a leap. */
#define PARALLAX_MAX_STEP_MS 1000

typedef struct {
    /* Accumulated motion in px, kept within one wrap period. */
    float offset;
} ParallaxLayerState;

struct Parallax {
    ClayCustomElement element;
    const ParallaxLayer* layers;
    ParallaxLayerState* states;
    size_t layer_count;
    uint16_t scene_width;
    int16_t origin_x;
    int16_t origin_y;
    float speed;
    uint32_t last_time_ms;
    bool has_last_time;
};

static bool parallax_is_moving(const Parallax* parallax) {
    if(parallax->speed != 0.f) return true;
    for(size_t i = 0; i < parallax->layer_count; i++) {
        const ParallaxLayer* layer = &parallax->layers[i];
        if(layer->swim != 0.f || layer->frame_count > 1) return true;
    }
    return false;
}

static float parallax_layer_period(const Parallax* parallax, const ParallaxLayer* layer) {
    switch(layer->wrap) {
    case ParallaxWrapTile:
        return layer->wrap_width;
    case ParallaxWrapPeriod:
        return (float)parallax->scene_width + layer->wrap_width;
    default:
        return 0.f;
    }
}

static void parallax_advance(Parallax* parallax, uint32_t now_ms) {
    uint32_t step_ms = 0;
    if(parallax->has_last_time) {
        step_ms = MIN(now_ms - parallax->last_time_ms, (uint32_t)PARALLAX_MAX_STEP_MS);
    }
    parallax->last_time_ms = now_ms;
    parallax->has_last_time = true;
    if(step_ms == 0) return;

    float dt = (float)step_ms / 1000.f;
    for(size_t i = 0; i < parallax->layer_count; i++) {
        const ParallaxLayer* layer = &parallax->layers[i];
        ParallaxLayerState* state = &parallax->states[i];

        state->offset += (parallax->speed * layer->speed_mult + layer->swim) * dt;

        float period = parallax_layer_period(parallax, layer);
        if(period > 0.f) state->offset = fmodf(state->offset, period);
    }
}

static const Image* parallax_layer_frame(const ParallaxLayer* layer, uint32_t now_ms) {
    furi_check(layer->frame_count > 0);
    if(layer->frame_count == 1 || layer->fps == 0) return layer->frames[0];
    uint32_t frame = (uint32_t)(((uint64_t)now_ms * layer->fps / 1000) % layer->frame_count);
    return layer->frames[frame];
}

static void parallax_render(Canvas* canvas, const Clay_BoundingBox* bounding_box, void* context) {
    Parallax* parallax = context;
    uint32_t now_ms = canvas_get_render_time(canvas);

    parallax_advance(parallax, now_ms);

    bool mirror = parallax->speed > 0.f;
    int32_t clip_left = bounding_box->x;
    int32_t clip_right = bounding_box->x + bounding_box->width;

    for(size_t i = 0; i < parallax->layer_count; i++) {
        const ParallaxLayer* layer = &parallax->layers[i];
        const Image* frame = parallax_layer_frame(layer, now_ms);
        bool mirror_x = layer->face_against_motion && mirror;

        int32_t x = parallax->origin_x + layer->x + (int32_t)lroundf(parallax->states[i].offset);
        int32_t y = parallax->origin_y + layer->y;

        switch(layer->wrap) {
        case ParallaxWrapNone:
            canvas_draw_image(canvas, x, y, frame, mirror_x);
            break;
        case ParallaxWrapTile:
            for(int32_t tile_x = parallax_tile_first_x(x, layer->wrap_width, clip_left); tile_x < clip_right; tile_x += layer->wrap_width) {
                canvas_draw_image(canvas, tile_x, y, frame, mirror_x);
            }
            break;
        case ParallaxWrapPeriod: {
            int32_t period = (int32_t)parallax->scene_width + layer->wrap_width;
            int32_t scene_x = parallax_period_x(x - parallax->origin_x, layer->wrap_width, period);
            canvas_draw_image(canvas, parallax->origin_x + scene_x, y, frame, mirror_x);
            break;
        }
        }
    }

    if(parallax_is_moving(parallax)) {
        canvas_request_redraw(canvas, PARALLAX_FRAME_PERIOD_MS);
    } else {
        /* Nothing moves: the next pass starts the clock afresh. */
        parallax->has_last_time = false;
    }
}

Parallax* parallax_alloc(const ParallaxLayer* layers, size_t layer_count, uint16_t scene_width) {
    furi_check(layers && layer_count > 0);
    for(size_t i = 0; i < layer_count; i++) {
        furi_check(layers[i].frames && layers[i].frame_count > 0);
        furi_check(layers[i].wrap == ParallaxWrapNone || layers[i].wrap_width > 0);
    }

    Parallax* parallax = malloc(sizeof(Parallax));
    parallax->element.render = parallax_render;
    parallax->element.context = parallax;
    parallax->layers = layers;
    parallax->states = malloc(sizeof(ParallaxLayerState) * layer_count);
    parallax->layer_count = layer_count;
    parallax->scene_width = scene_width;
    return parallax;
}

void parallax_free(Parallax* parallax) {
    furi_check(parallax);
    free(parallax->states);
    free(parallax);
}

void parallax_set_origin(Parallax* parallax, int16_t x, int16_t y) {
    furi_check(parallax);
    parallax->origin_x = x;
    parallax->origin_y = y;
}

void parallax_set_speed(Parallax* parallax, float speed) {
    furi_check(parallax);
    parallax->speed = speed;
}

float parallax_get_speed(const Parallax* parallax) {
    furi_check(parallax);
    return parallax->speed;
}

void parallax_element(Clay_ElementId id, Parallax* parallax) {
    furi_check(parallax);
    CLAY(
        id,
        {
            .layout =
                {
                    .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)},
                },
            .custom = {.customData = &parallax->element},
        }) {
    }
}
