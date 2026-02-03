#pragma once
#include <gui_test/clay.h>
#include <gui_test/clay_render.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CLAY_APP_ID(x) CLAY_ID(TAG x)

static const Clay_Color COLOR_WHITE = {255, 255, 255, 255};

static const Clay_Color COLOR_BLACK = {0, 0, 0, 255};

bool clay_helper_scroll_to_child(Clay_ElementId scrollContainerId, Clay_ElementId childId, int32_t paddingX, int32_t paddingY, int32_t speed);

Clay_String clay_helper_string_from(FuriString* furi_string);

#ifdef __cplusplus
}
#endif