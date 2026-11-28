#pragma once
#include "desktop.h"
#include <input/input.h>

typedef struct Desktop Desktop;

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DesktopSceneEventTypeTogglePowerMenu,
    DesktopSceneEventTypeToggleHelpMenu,
    DesktopSceneEventTypeReturnToDesktop,
    DesktopSceneEventTypeEnterSettingsMenu,

    DesktopSceneEventTypeEnterDisplaySettings,
    DesktopSceneEventTypeEnterPowerSettings,
    DesktopSceneEventTypeEnterTestingMenu,
    DesktopSceneEventTypeEnterLedsMenu,

    DesktopSceneEventTypeOpenDebugMenu,

    DesktopSceneEventTypePowerUpdate,

    DesktopSceneEventTypeStartSelfCheckApp,
    DesktopSceneEventTypeStartMaskromApp,
    DesktopSceneEventTypeStartCpuApp,
    DesktopSceneEventTypeStartKeypadApp,
    DesktopSceneEventTypeStartTouchpadApp,
    DesktopSceneEventTypeStartHapticApp,
    DesktopSceneEventTypeStartDisplayTestApp,

    DesktopSceneEventTypeStartAppById,

} DesktopSceneEvent;

void desktop_send_scene_event(Desktop* desktop, uint32_t event, void* data);

/** A home screen tab: the menu that opens from it draws it active with the same values */
typedef struct {
    size_t index;
    const char* label;
    InputKey key;
} DesktopHomeTab;

extern const DesktopHomeTab desktop_home_tab_help;
extern const DesktopHomeTab desktop_home_tab_power;
extern const DesktopHomeTab desktop_home_tab_settings;

void desktop_power_off(void);

#ifdef __cplusplus
}
#endif
