#include <furi.h>
#include "clay_helper.h"

#define TAG "ClayHelpers"

bool clay_helper_scroll_to_child(Clay_ElementId scrollContainerId, Clay_ElementId childId, int32_t paddingX, int32_t paddingY, int32_t speed) {
    // Get scroll container data
    Clay_ScrollContainerData scrollData = Clay_GetScrollContainerData(scrollContainerId);
    if(!scrollData.found) {
        FURI_LOG_E(TAG, "Couldn't find scroll state. Does it have a scroll config?\n");
        return false;
    }

    // Get scroll container bounding box
    Clay_ElementData scrollContainerData = Clay_GetElementData(scrollContainerId);
    if(scrollContainerData.found == false) {
        FURI_LOG_E(TAG, "Couldn't find layout element for scroll container\n");
        return false;
    }
    Clay_BoundingBox scrollContainerBounds = scrollContainerData.boundingBox;

    // Get child bounding box
    Clay_ElementData childData = Clay_GetElementData(childId);
    if(childData.found == false) {
        FURI_LOG_E(TAG, "Couldn't find layout element for child\n");
        return false;
    }
    Clay_BoundingBox childBounds = childData.boundingBox;

    // Calculate child's position relative to scroll content
    int32_t relativeX = childBounds.x - scrollData.scrollPosition->x - scrollContainerBounds.x;
    int32_t relativeY = childBounds.y - scrollData.scrollPosition->y - scrollContainerBounds.y;

    int32_t contentPosX = relativeX - paddingX;
    int32_t contentPosY = relativeY - paddingY;
    int32_t contentWidth = childBounds.width + paddingX * 2;
    int32_t contentHeight = childBounds.height + paddingY * 2;

    // Get current scroll position and container dimensions
    int32_t scrollX = scrollData.scrollPosition->x;
    int32_t scrollY = scrollData.scrollPosition->y;
    int32_t containerWidth = scrollData.scrollContainerDimensions.width;
    int32_t containerHeight = scrollData.scrollContainerDimensions.height;

    // If element goes beyond the right edge
    if(contentPosX + contentWidth > -scrollX + containerWidth) {
        scrollData.scrollPosition->x -= (contentPosX + contentWidth) - (-scrollX + containerWidth);
    }

    // If element goes beyond the left edge
    if(contentPosX < -scrollX) {
        scrollData.scrollPosition->x -= contentPosX + scrollX;
    }

    // If element goes beyond the bottom edge
    if(contentPosY + contentHeight > -scrollY + containerHeight) {
        int32_t scroll = (contentPosY + contentHeight) - (-scrollY + containerHeight);
        if(speed > 0) scroll = (scroll > speed) ? speed : scroll;
        scrollData.scrollPosition->y -= scroll;
    }

    // If element goes beyond the top edge
    if(contentPosY < -scrollY) {
        int32_t scroll = contentPosY + scrollY;
        if(speed > 0) scroll = (-scroll > speed) ? -speed : scroll;
        scrollData.scrollPosition->y -= scroll;
    }

    if(scrollX != scrollData.scrollPosition->x || scrollY != scrollData.scrollPosition->y) {
        return true;
    } else {
        return false;
    }
}
