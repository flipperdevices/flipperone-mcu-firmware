#pragma once
#include "desktop.h"

typedef struct Desktop Desktop;

#ifdef __cplusplus
extern "C" {
#endif

void desktop_show_power_menu(Desktop* desktop);
void desktop_hide_power_menu(Desktop* desktop);
bool desktop_get_power_menu_state(Desktop* desktop);

#ifdef __cplusplus
}
#endif
