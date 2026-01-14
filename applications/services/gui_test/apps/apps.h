#pragma once
#include "../app_common.h"

extern App* const apps[];
extern const size_t app_count;

void apps_call_render(App* app);

bool apps_call_input(App* app, const GuiTestMessage* message);

void apps_call_scroll(App* app);

void render_do_render(Clay_RenderCommandArray* renderCommands);