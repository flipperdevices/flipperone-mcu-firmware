#include "apps.h"

extern App app_test_keypad;
extern App app_test_touchpad;
extern App app_playdate;

App* const apps[] = {
    &app_test_keypad,
    &app_test_touchpad,
    &app_playdate,
    &app_test_keypad,
    &app_test_touchpad,
    &app_playdate,
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

void render_do_render(Clay_RenderCommandArray* renderCommands) {
    for(int i = 0; i < renderCommands->length; i++) {
        Clay_RenderCommand* renderCommand = &renderCommands->internalArray[i];
        Clay_BoundingBox boundingBox = renderCommand->boundingBox;

        switch(renderCommand->commandType) {
        case CLAY_RENDER_COMMAND_TYPE_RECTANGLE:
            render_rectangle(&boundingBox, &renderCommand->renderData.rectangle);
            break;
        case CLAY_RENDER_COMMAND_TYPE_BORDER:
            render_border(&boundingBox, &renderCommand->renderData.border);
            break;
        case CLAY_RENDER_COMMAND_TYPE_TEXT:
            render_text(&boundingBox, &renderCommand->renderData.text);
            break;
        case CLAY_RENDER_COMMAND_TYPE_IMAGE:
            render_image(&boundingBox, &renderCommand->renderData.image);
            break;
        case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START:
            render_scissor_start(&boundingBox);
            break;
        case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END:
            render_scissor_end();
            break;
        case CLAY_RENDER_COMMAND_TYPE_CUSTOM:
            furi_crash("Custom render commands are not supported");
            break;
        }
    }
}
