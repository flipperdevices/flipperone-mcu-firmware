#pragma once
#include "../app_common.h"

void apps_call_render(App* app);

bool apps_call_input(App* app, const GuiTestMessage* message);

void apps_call_scroll(App* app);
