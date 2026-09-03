#pragma once
#include "desktop.h"

typedef struct Desktop Desktop;

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DesktopSceneEventTypeTogglePowerMenu,
    DesktopSceneEventTypeReturnToDesktop,
    DesktopSceneEventTypeEnterSettingsMenu,

    DesktopSceneEventTypeEnterDisplaySettings,
    DesktopSceneEventTypeEnterPowerSettings,
    DesktopSceneEventTypeEnterTestingMenu,
    DesktopSceneEventTypeEnterLedsMenu,

    DesktopSceneEventTypeOpenDebugMenu,

    DesktopSceneEventTypePowerUpdate,

} DesktopSceneEvent;

void desktop_send_scene_event(Desktop* desktop, uint32_t event, void* data);

void desktop_start_cpu(bool to_maskrom);

/** Start an app from FLIPPER_APPS by its appid. Returns false if not found. */
bool desktop_start_app_by_id(const char* appid);

void desktop_power_off(void);

#ifdef __cplusplus
}
#endif
