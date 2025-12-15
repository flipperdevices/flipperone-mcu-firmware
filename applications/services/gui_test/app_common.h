#include <input/input.h>
#include <input_touch/input_touch.h>
#include <gui_test/clay_render.h>

typedef enum {
    GuiTestMessageTypeInputEvent,
    GuiTestMessageTypeInputTouchEvent,
} GuiTestMessageType;

typedef struct {
    GuiTestMessageType type;
    union {
        InputEvent input_event;
        InputTouchEvent input_touch_event;
    };
} GuiTestMessage;

static const Clay_Color COLOR_WHITE = {255, 255, 255, 255};
static const Clay_Color COLOR_BLACK = {0, 0, 0, 255};

typedef struct App App;

struct App {
    const void* const state;
    const bool (*const input)(App* app, const GuiTestMessage* message);
    const void (*const render)(App* app);
    const void (*const scroll)(App* app);
};

#define CLAY_APP_ID(x) CLAY_ID(TAG x)
