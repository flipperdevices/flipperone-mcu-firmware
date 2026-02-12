#pragma once
#include "clay.h"
#include "clay_render.h"

#ifdef __cplusplus
extern "C" {
#endif

// Macro for defining application element IDs with a prefix based on the application TAG
#define CLAY_APP_ID(x) CLAY_ID(TAG x)

static const Clay_Color COLOR_WHITE = {255, 255, 255, 255};

static const Clay_Color COLOR_BLACK = {0, 0, 0, 255};

/**
 * @brief Scrolls a scroll container to make a child element visible, with optional padding and speed.
 * 
 * @param scrollContainerId The ID of the scroll container element.
 * @param childId The ID of the child element to scroll to.
 * @param paddingX Horizontal padding around the child element.
 * @param paddingY Vertical padding around the child element.
 * @param speed Scrolling speed. If positive, limits the maximum scroll amount per call.
 * @return true if scrolling occurred,
 * @return false if no scrolling was needed.
 */
bool clay_helper_scroll_to_child(Clay_ElementId scrollContainerId, Clay_ElementId childId, int32_t paddingX, int32_t paddingY, int32_t speed);

/**
 * @brief Constructs a Clay_String from a FuriString.
 * @warning The underlying character data from the FuriString must remain valid for the lifetime of the Clay_String.
 * @param furi_string The FuriString to convert.
 * @return Clay_String 
 */
Clay_String clay_helper_string_from(FuriString* furi_string);

/**
 * @brief Constructs a Clay_String from a null-terminated C string.
 * @warning The underlying character data from the C string must remain valid for the lifetime of the Clay_String.
 * @param chars The null-terminated C string to convert.
 * @return Clay_String
 */
Clay_String clay_helper_string_from_chars(const char* chars);

#ifdef __cplusplus
}
#endif