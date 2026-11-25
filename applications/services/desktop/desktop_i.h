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

/** Whether the home screen scene is shown (possibly under an overlay such as
 *  the power menu), as opposed to a settings or debug scene. */
bool desktop_is_home_shown(Desktop* desktop);

void desktop_power_off(void);

#ifdef __cplusplus
}
#endif
