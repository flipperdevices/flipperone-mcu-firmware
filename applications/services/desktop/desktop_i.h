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

void desktop_power_off(void);

#ifdef __cplusplus
}
#endif
