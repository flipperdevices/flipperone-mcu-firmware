#include <furi.h>
#include "clay_helper.h"

#define TAG "ClayHelpers"

bool clay_helper_scroll_to_child(Clay_ElementId scrollContainerId, Clay_ElementId childId, int32_t paddingX, int32_t paddingY, int32_t speed) {
    // based on https://github.com/nicbarker/clay/issues/346

    // get scroll container data
    Clay_ScrollContainerData scrollData = Clay_GetScrollContainerData(scrollContainerId);
    if(!scrollData.found) {
        FURI_LOG_E(TAG, "Couldn't find scroll state. Does it have a scroll config?\n");
        return false;
    }

    // get scroll container bounding box
    Clay_ElementData scrollContainerData = Clay_GetElementData(scrollContainerId);
    if(scrollContainerData.found == false) {
        FURI_LOG_E(TAG, "Couldn't find layout element for scroll container\n");
        return false;
    }
    Clay_BoundingBox scrollContainerBounds = scrollContainerData.boundingBox;

    // get child bounding box
    Clay_ElementData childData = Clay_GetElementData(childId);
    if(childData.found == false) {
        FURI_LOG_E(TAG, "Couldn't find layout element for child\n");
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
