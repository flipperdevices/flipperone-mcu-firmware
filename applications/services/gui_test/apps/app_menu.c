
#include <gui_test/app_common.h>
#include <drivers/display/display_jd9853_reg.h>

#define TAG "AppMenu"

typedef struct {
    const char* text;
} MenuItem;

typedef struct {
    uint32_t length;
    int32_t selected_index;
    MenuItem* items;
} MenuAppState;

static bool Clay_ScrollToChild(Clay_ElementId scrollContainerId, Clay_ElementId childId, int32_t paddingX, int32_t paddingY, int32_t speed) {
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

    return true;
}

static Clay_String test_str = {
    .isStaticallyAllocated = false,
    .length = 0,
    .chars = NULL,
};

static void menu_layout(App* app) {
    furi_assert(app);
    MenuAppState* state = (MenuAppState*)app->state;

    Clay_Sizing layoutExpand = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};

    CLAY(
        CLAY_APP_ID("Container"),
        {
            .backgroundColor = COLOR_WHITE,
            .layout =
                {
                    .layoutDirection = CLAY_TOP_TO_BOTTOM,
                    .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)},
                    .childGap = 4,
                    .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
                },
            .clip = {.vertical = true, .childOffset = Clay_GetScrollOffset()},
        }) {
        for(uint32_t i = 0; i < state->length; i++) {
            bool selected = (i == state->selected_index);
            CLAY(
                CLAY_SIDI(CLAY_STRING("Menu"), i),
                {
                    .layout =
                        {
                            .sizing = {.width = CLAY_SIZING_FIXED(120), .height = CLAY_SIZING_FIXED(13)},
                            .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
                        },
                    .backgroundColor = selected ? COLOR_BLACK : COLOR_WHITE,
                    .cornerRadius = CLAY_CORNER_RADIUS(2),
                }) {
                test_str.chars = state->items[i].text;
                test_str.length = strlen(state->items[i].text);
                CLAY_TEXT(test_str, CLAY_TEXT_CONFIG({.fontId = FontBody, .textColor = selected ? COLOR_WHITE : COLOR_BLACK}));
            }
        }
    }
}

static bool menu_input(App* app, const GuiTestMessage* message) {
    furi_assert(app);
    furi_assert(message);

    MenuAppState* state = (MenuAppState*)app->state;
    bool handled = false;

    switch(message->type) {
    case GuiTestMessageTypeInputEvent: {
        InputEvent event = message->input_event;
        if(event.key == InputKeyDown) {
            if(event.type == InputTypePress) {
                state->selected_index = (state->selected_index + 1) % state->length;
                handled = true;
            }
        }
        if(event.key == InputKeyUp) {
            if(event.type == InputTypePress) {
                state->selected_index = (state->selected_index - 1 + state->length) % state->length;
                handled = true;
            }
        }
    } break;
    case GuiTestMessageTypeInputTouchEvent: {
    } break;
    }

    return handled;
}

static void menu_scroll(App* app) {
    furi_assert(app);
    MenuAppState* state = (MenuAppState*)app->state;
    UNUSED(state);

    Clay_ElementId scrollContainerId = CLAY_APP_ID("Container");
    Clay_ElementId targetChildId = CLAY_SIDI(CLAY_STRING("Menu"), state->selected_index);
    Clay_ScrollToChild(scrollContainerId, targetChildId, 0, 10, 15);
}

static const MenuItem menu_items[] = {
    {.text = "Nmap"},
    {.text = "LED test"},
    {.text = "PTT test"},
    {.text = "Settings"},
    {.text = "Services"},
    {.text = "Config"},
    {.text = "About"},
    {.text = "Empty line"},
    {.text = "Exit"},
    {.text = "Touch test"},
    {.text = "Diagnostics"},
    {.text = "Update firmware"},
    {.text = "Reboot"},
};

const App app_menu = {
    .state =
        &(MenuAppState){
            .items = (MenuItem*)menu_items,
            .length = COUNT_OF(menu_items),
            .selected_index = 0,
        },
    .input = menu_input,
    .render = menu_layout,
    .scroll = menu_scroll,
};
