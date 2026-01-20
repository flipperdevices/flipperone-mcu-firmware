#include "apps.h"

extern App app_test_keypad;
extern App app_test_touchpad;
extern App app_complex;
extern App app_console;

App* const apps[] = {
    &app_test_keypad,
    &app_test_touchpad,
    &app_complex,
    &app_console,
};

const size_t app_count = COUNT_OF(apps);

void apps_call_render(App* app) {
    if(app) {
        if(app->render) {
            app->render(app);
        }
    }
}

bool apps_call_input(App* app, const GuiTestMessage* message) {
    bool handled = false;
    if(app) {
        if(app->input) {
            handled = app->input(app, message);
        }
    }
    return handled;
}

void apps_call_scroll(App* app) {
    if(app) {
        if(app->scroll) {
            app->scroll(app);
        }
    }
}
