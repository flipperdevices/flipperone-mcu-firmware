#include "apps.h"

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
