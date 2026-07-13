#pragma once
#include <furi.h>

#define RECORD_POWER_MENU "power_menu"

typedef void (*PowerMenuClickCallback)(bool pressed, void* context);
typedef struct {
    PowerMenuClickCallback callback;
    void* context;
} PowerMenuClickWithContext;

#ifdef __cplusplus
extern "C" {
#endif

bool power_menu_add_menu_item(const char* text, PowerMenuClickWithContext on_click);
bool power_menu_remove_menu_item(const char* text);

#ifdef __cplusplus
}
#endif
