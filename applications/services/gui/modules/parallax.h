#pragma once
#include <gui/clay_helper.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Parallax scene: a stack of image layers that slide across the screen at
 * different shares of one scene speed, each wrapping in its own way. Drawn as
 * a Clay custom element (parallax_element()); while anything in it moves or
 * changes frame, it asks the GUI for a redraw at its next visible change.
 *
 * Scene coordinates are screen coordinates shifted by parallax_set_origin():
 * the element only clips, it does not move the scene, so an artist's layout
 * holds whatever box the scene is shown through.
 */

typedef enum {
    /** Drawn once, at its position plus the accumulated offset. */
    ParallaxWrapNone,
    /** Repeats every wrap_width px, so copies cover the whole scene width. */
    ParallaxWrapTile,
    /** One copy that leaves through one edge of the scene and comes back in
     *  through the other: its period is scene_width + wrap_width. */
    ParallaxWrapPeriod,
} ParallaxWrap;

typedef enum {
    ParallaxShowAlways,
    /** Only while the scene speed is 0 and no layer is still arriving. */
    ParallaxShowStill,
    /** Only while the scene moves and no layer is still arriving. */
    ParallaxShowMoving,
    /** Only while this layer is on its way in (see arrive_from). */
    ParallaxShowArriving,
} ParallaxShow;

/** One sprite of a composite frame, placed inside the frame's box. */
typedef struct {
    const Image* image;
    int16_t x;
    int16_t y;
    /** 255 opaque; lower values blend the sprite over what is already drawn. */
    uint8_t alpha;
} ParallaxSprite;

/** A frame built from several sprites: an artist's canvas where each frame
 *  places its parts at its own offsets (a bobbing sprite, its bubbles). */
typedef struct {
    const ParallaxSprite* sprites;
    uint8_t sprite_count;
} ParallaxComposite;

typedef struct {
    /** frames[floor(t * fps) % frame_count], phase-locked to the clock. Any
     *  image format the renderer draws.
     *  Either this or composites is set, never both. */
    const Image* const* frames;
    /** Composite frames instead of plain images, same indexing; their box is
     *  wrap_width wide, which mirroring and the wraps refer to. */
    const ParallaxComposite* composites;
    uint8_t frame_count;
    /** Frame rate of the sequence; ignored with a single frame. */
    uint8_t fps;
    /** Scene position of the layer at offset 0. */
    int16_t x;
    int16_t y;
    /** Share of the scene speed this layer moves at (1 = foreground, 0 = still). */
    float speed_mult;
    /** The layer's own motion in px/s, positive to the right, added to the scene's. */
    float swim;
    ParallaxWrap wrap;
    /** Width the wrap repeats with: the widest frame, so a narrower one does not jitter. */
    uint16_t wrap_width;
    /** Mirror the frames horizontally while the scene moves right, so the
     *  sprite always faces against the motion (the source art faces right). */
    bool face_against_motion;
    /** When the layer is drawn at all: a sleeping and a swimming pose of one
     *  character are two layers that take turns. */
    ParallaxShow show;
    /** An entrance: the layer starts arrive_from px away from its position
     *  (negative = to the left) and closes in at arrive_speed px/s, once,
     *  from the scene's first frame. Wrap must be None. */
    int16_t arrive_from;
    float arrive_speed;
} ParallaxLayer;

typedef struct Parallax Parallax;

/** @param layers       drawn first to last (bottom to top); the array must outlive the scene
 *  @param scene_width  width the Period wrap and the default origin refer to */
Parallax* parallax_alloc(const ParallaxLayer* layers, size_t layer_count, uint16_t scene_width);

void parallax_free(Parallax* parallax);

/** Screen position of the scene's (0, 0). Default (0, 0). */
void parallax_set_origin(Parallax* parallax, int16_t x, int16_t y);

/** Scene speed in px/s, positive moves the scene to the right. Offsets
 *  accumulate across changes, so a new speed never jumps the picture. */
void parallax_set_speed(Parallax* parallax, float speed);

float parallax_get_speed(const Parallax* parallax);

/** Place the scene in the layout. The element grows to fill its parent and
 *  clips the scene to that box. */
void parallax_element(Clay_ElementId id, Parallax* parallax);

/* ── Wrap arithmetic ─────────────────────────────────────────────────────── */

/** Mirroring a `box_width` wide frame in place: where a `width` wide sprite
 *  at `x` inside it lands. */
static inline int32_t parallax_mirror_x(int32_t x, int32_t width, int32_t box_width) {
    return box_width - x - width;
}

/** Period wrap: where a sprite of `width` whose unwrapped position is `x`
 *  shows up, in [-width, period - width). */
static inline int32_t parallax_period_x(int32_t x, int32_t width, int32_t period) {
    int32_t wrapped = (x + width) % period;
    if(wrapped < 0) wrapped += period;
    return wrapped - width;
}

/** Tile wrap: the leftmost copy of a `width`-wide tile at unwrapped position
 *  `x` that still reaches `left`, in (left - width, left]. */
static inline int32_t parallax_tile_first_x(int32_t x, int32_t width, int32_t left) {
    int32_t rel = (x - left) % width;
    if(rel > 0) rel -= width;
    return left + rel;
}

#ifdef __cplusplus
}
#endif
