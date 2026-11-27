#include "parallax.h"
#include <furi.h>
#include <math.h>
#include <string.h>

#define TAG "Parallax"

/* Redraws come at most this often, however fast something moves. */
#define PARALLAX_MIN_FRAME_MS 33

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
    /* Set from any thread; a render pass works with its own copy in speed,
     * so its layers all see one value. */
    float requested_speed;
    float speed;
    uint32_t last_time_ms;
    bool has_last_time;
};

static bool parallax_layer_arriving(const Parallax* parallax, size_t i) {
    return parallax->layers[i].arrive_speed > 0.f && parallax->states[i].offset != 0.f;
}

static bool parallax_is_arriving(const Parallax* parallax) {
    for(size_t i = 0; i < parallax->layer_count; i++) {
        if(parallax_layer_arriving(parallax, i)) return true;
    }
    return false;
}

static bool parallax_layer_shown(const Parallax* parallax, size_t i, bool arriving) {
    switch(parallax->layers[i].show) {
    case ParallaxShowStill:
        return parallax->speed == 0.f && !arriving;
    case ParallaxShowMoving:
        return parallax->speed != 0.f && !arriving;
    case ParallaxShowArriving:
        return parallax_layer_arriving(parallax, i);
    default:
        return true;
    }
}

/* Its motion in px/s right now, positive to the right. */
static float parallax_layer_velocity(const Parallax* parallax, size_t i) {
    const ParallaxLayer* layer = &parallax->layers[i];
    if(layer->arrive_speed > 0.f) {
        const float offset = parallax->states[i].offset;
        return offset < 0.f ? layer->arrive_speed : offset > 0.f ? -layer->arrive_speed : 0.f;
    }
    return parallax->speed * layer->speed_mult + layer->swim;
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

        if(layer->arrive_speed > 0.f) {
            /* On the way in: close in on 0 and stop there. */
            float step = layer->arrive_speed * dt;
            if(state->offset < 0.f) {
                state->offset = MIN(state->offset + step, 0.f);
            } else if(state->offset > 0.f) {
                state->offset = MAX(state->offset - step, 0.f);
            }
            continue;
        }

        state->offset += (parallax->speed * layer->speed_mult + layer->swim) * dt;

        float period = parallax_layer_period(parallax, layer);
        if(period > 0.f) state->offset = fmodf(state->offset, period);
    }
}

static uint32_t parallax_layer_frame(const ParallaxLayer* layer, uint32_t now_ms) {
    furi_check(layer->frame_count > 0);
    if(layer->frame_count == 1 || layer->fps == 0) return 0;
    return (uint32_t)(((uint64_t)now_ms * layer->fps / 1000) % layer->frame_count);
}

/* Time until the layer looks different: its next frame, or its next pixel of
 * motion (or the end of its entrance). UINT32_MAX if it stands still. */
static uint32_t parallax_layer_next_change_ms(const Parallax* parallax, size_t i, uint32_t now_ms) {
    const ParallaxLayer* layer = &parallax->layers[i];
    uint32_t delay_ms = UINT32_MAX;

    if(layer->frame_count > 1 && layer->fps > 0) {
        const uint64_t next_frame = (uint64_t)now_ms * layer->fps / 1000 + 1;
        const uint64_t next_frame_ms = (next_frame * 1000 + layer->fps - 1) / layer->fps;
        delay_ms = (uint32_t)(next_frame_ms - now_ms);
    }

    const float velocity = parallax_layer_velocity(parallax, i);
    if(velocity != 0.f) {
        const float offset = parallax->states[i].offset;
        // Drawn at the rounded offset: the next pixel is at the next half
        const float edge = velocity > 0.f ? roundf(offset) + 0.5f : roundf(offset) - 0.5f;
        float distance = fabsf(edge - offset);
        if(layer->arrive_speed > 0.f) distance = MIN(distance, fabsf(offset));
        delay_ms = MIN(delay_ms, (uint32_t)ceilf(distance * 1000.f / fabsf(velocity)));
    }
    return delay_ms;
}

/* Draw frame `frame` of the layer with its box's top-left at (x, y). */
static void parallax_draw_frame(Canvas* canvas, const ParallaxLayer* layer, uint32_t frame, int32_t x, int32_t y, bool mirror_x) {
    if(layer->frames) {
        canvas_draw_image(canvas, x, y, layer->frames[frame], mirror_x, 255);
        return;
    }
    const ParallaxComposite* composite = &layer->composites[frame];
    for(uint8_t i = 0; i < composite->sprite_count; i++) {
        const ParallaxSprite* sprite = &composite->sprites[i];
        int32_t sprite_x = sprite->x;
        if(mirror_x) sprite_x = parallax_mirror_x(sprite_x, sprite->image->width, layer->wrap_width);
        canvas_draw_image(canvas, x + sprite_x, y + sprite->y, sprite->image, mirror_x, sprite->alpha);
    }
}

static void parallax_render(Canvas* canvas, const Clay_BoundingBox* bounding_box, void* context) {
    Parallax* parallax = context;
    uint32_t now_ms = canvas_get_render_time(canvas);

    parallax->speed = parallax->requested_speed;
    parallax_advance(parallax, now_ms);

    bool mirror = parallax->speed > 0.f;
    bool arriving = parallax_is_arriving(parallax);
    int32_t clip_left = bounding_box->x;
    int32_t clip_right = bounding_box->x + bounding_box->width;
    uint32_t next_change_ms = UINT32_MAX;

    for(size_t i = 0; i < parallax->layer_count; i++) {
        const ParallaxLayer* layer = &parallax->layers[i];
        if(!parallax_layer_shown(parallax, i, arriving)) continue;
        next_change_ms = MIN(next_change_ms, parallax_layer_next_change_ms(parallax, i, now_ms));
        uint32_t frame = parallax_layer_frame(layer, now_ms);
        bool mirror_x = layer->face_against_motion && mirror;

        int32_t x = parallax->origin_x + layer->x + (int32_t)lroundf(parallax->states[i].offset);
        int32_t y = parallax->origin_y + layer->y;

        switch(layer->wrap) {
        case ParallaxWrapNone:
            parallax_draw_frame(canvas, layer, frame, x, y, mirror_x);
            break;
        case ParallaxWrapTile:
            for(int32_t tile_x = parallax_tile_first_x(x, layer->wrap_width, clip_left); tile_x < clip_right; tile_x += layer->wrap_width) {
                parallax_draw_frame(canvas, layer, frame, tile_x, y, mirror_x);
            }
            break;
        case ParallaxWrapPeriod: {
            int32_t period = (int32_t)parallax->scene_width + layer->wrap_width;
            int32_t scene_x = parallax_period_x(x - parallax->origin_x, layer->wrap_width, period);
            parallax_draw_frame(canvas, layer, frame, parallax->origin_x + scene_x, y, mirror_x);
            break;
        }
        }
    }

    if(next_change_ms != UINT32_MAX) {
        canvas_request_redraw(canvas, MAX(next_change_ms, (uint32_t)PARALLAX_MIN_FRAME_MS));
    } else {
        /* Nothing changes: the next pass starts the clock afresh. */
        parallax->has_last_time = false;
    }
}

Parallax* parallax_alloc(const ParallaxLayer* layers, size_t layer_count, uint16_t scene_width) {
    furi_check(layers && layer_count > 0);
    for(size_t i = 0; i < layer_count; i++) {
        furi_check((layers[i].frames != NULL) != (layers[i].composites != NULL));
        furi_check(layers[i].frame_count > 0);
        furi_check((layers[i].wrap == ParallaxWrapNone && layers[i].frames) || layers[i].wrap_width > 0);
        furi_check(layers[i].arrive_speed == 0.f || layers[i].wrap == ParallaxWrapNone);
    }

    // malloc does not clear
    Parallax* parallax = malloc(sizeof(Parallax));
    memset(parallax, 0, sizeof(Parallax));
    parallax->element.render = parallax_render;
    parallax->element.context = parallax;
    parallax->layers = layers;
    parallax->states = malloc(sizeof(ParallaxLayerState) * layer_count);
    parallax->layer_count = layer_count;
    parallax->scene_width = scene_width;
    for(size_t i = 0; i < layer_count; i++) {
        parallax->states[i].offset = layers[i].arrive_speed > 0.f ? layers[i].arrive_from : 0.f;
    }
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
    parallax->requested_speed = speed;
}

float parallax_get_speed(const Parallax* parallax) {
    furi_check(parallax);
    return parallax->requested_speed;
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
