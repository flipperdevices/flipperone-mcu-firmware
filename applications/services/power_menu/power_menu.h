#pragma once
#include <furi.h>
#include <toolbox/furi_callback.h>

#define RECORD_POWER_MENU "power_menu"

#ifdef __cplusplus
extern "C" {
#endif

bool power_menu_add_menu_item(const char* text, FuriCallbackWithContext on_click);
bool power_menu_remove_menu_item(const char* text);

#ifdef __cplusplus
}
#endif
