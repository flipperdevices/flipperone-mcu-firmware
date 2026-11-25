#include <furi.h>
#include "clay_helper.h"

#define TAG "ClayHelpers"

bool clay_helper_scroll_to_child(Clay_ElementId scrollContainerId, Clay_ElementId childId, int32_t paddingX, int32_t paddingY, int32_t speed) {
    // based on https://github.com/nicbarker/clay/issues/346

    // get scroll container data
    Clay_ScrollContainerData scrollData = Clay_GetScrollContainerData(scrollContainerId);
    if(!scrollData.found) {
        FURI_LOG_E(TAG, "Couldn't find scroll state. Does it have a scroll config?");
        return false;
    }

    // get scroll container bounding box
    Clay_ElementData scrollContainerData = Clay_GetElementData(scrollContainerId);
    if(scrollContainerData.found == false) {
        FURI_LOG_E(TAG, "Couldn't find layout element for scroll container");
        return false;
    }
    Clay_BoundingBox scrollContainerBounds = scrollContainerData.boundingBox;

    // get child bounding box
    Clay_ElementData childData = Clay_GetElementData(childId);
    if(childData.found == false) {
        FURI_LOG_E(TAG, "Couldn't find layout element for child");
        return false;
    }
    Clay_BoundingBox childBounds = childData.boundingBox;

    // calculate child's position relative to scroll content
    int32_t relativeX = childBounds.x - scrollData.scrollPosition->x - scrollContainerBounds.x;
    int32_t relativeY = childBounds.y - scrollData.scrollPosition->y - scrollContainerBounds.y;

    int32_t contentPosX = relativeX - paddingX;
    int32_t contentPosY = relativeY - paddingY;
    int32_t contentWidth = childBounds.width + paddingX * 2;
    int32_t contentHeight = childBounds.height + paddingY * 2;

    // get current scroll position and container dimensions
    int32_t scrollX = scrollData.scrollPosition->x;
    int32_t scrollY = scrollData.scrollPosition->y;
    int32_t containerWidth = scrollData.scrollContainerDimensions.width;
    int32_t containerHeight = scrollData.scrollContainerDimensions.height;

    bool scroll_happened = false;

    // element goes beyond the right edge
    if(contentPosX + contentWidth > -scrollX + containerWidth) {
        int32_t scroll = (contentPosX + contentWidth) - (-scrollY + containerWidth);
        if(speed > 0) scroll = (scroll > speed) ? speed : scroll;
        scrollData.scrollPosition->x -= scroll;
        scroll_happened = true;
    }

    // element goes beyond the left edge
    if(contentPosX < -scrollX) {
        int32_t scroll = contentPosX + scrollX;
        if(speed > 0) scroll = (-scroll > speed) ? -speed : scroll;
        scrollData.scrollPosition->x -= scroll;
        scroll_happened = true;
    }

    // element goes beyond the bottom edge
    if(contentPosY + contentHeight > -scrollY + containerHeight) {
        int32_t scroll = (contentPosY + contentHeight) - (-scrollY + containerHeight);
        if(speed > 0) scroll = (scroll > speed) ? speed : scroll;
        scrollData.scrollPosition->y -= scroll;
        scroll_happened = true;
    }

    // element goes beyond the top edge
    if(contentPosY < -scrollY) {
        int32_t scroll = contentPosY + scrollY;
        if(speed > 0) scroll = (-scroll > speed) ? -speed : scroll;
        scrollData.scrollPosition->y -= scroll;
        scroll_happened = true;
    }

    return scroll_happened;
}

Clay_String clay_helper_string_from(FuriString* furi_string) {
    Clay_String clay_string;
    clay_string.isStaticallyAllocated = false;
    clay_string.length = furi_string_size(furi_string);
    clay_string.chars = furi_string_get_cstr(furi_string);
    return clay_string;
}

Clay_String clay_helper_string_from_chars(const char* chars) {
    Clay_String clay_string;
    clay_string.isStaticallyAllocated = false;
    clay_string.length = strlen(chars);
    clay_string.chars = chars;
    return clay_string;
}

void clay_fixed_image(Clay_ElementId id, const Image* image) {
    CLAY(
        id,
        {
            .layout =
                {
                    .sizing = {.height = CLAY_SIZING_FIXED(image->height), .width = CLAY_SIZING_FIXED(image->width)},
                },
            .image = {.imageData = (void*)image},
        }) {
    }
}

/* Layout runs before render on the same thread, so one slot collects the
 * shortest delay of a frame and the render pass consumes it. */
static uint32_t clay_redraw_delay_ms = UINT32_MAX;

static void clay_request_redraw_render(Canvas* canvas, const Clay_BoundingBox* bounding_box, void* context) {
    UNUSED(bounding_box);
    UNUSED(context);
    if(clay_redraw_delay_ms != UINT32_MAX) {
        canvas_request_redraw(canvas, clay_redraw_delay_ms);
        clay_redraw_delay_ms = UINT32_MAX;
    }
}

static const ClayCustomElement clay_request_redraw_element = {.render = clay_request_redraw_render, .context = NULL};

void clay_request_redraw(Clay_ElementId id, uint32_t delay_ms) {
    if(delay_ms < clay_redraw_delay_ms) clay_redraw_delay_ms = delay_ms;
    CLAY(id, {.custom = {.customData = (void*)&clay_request_redraw_element}}) {
    }
}
