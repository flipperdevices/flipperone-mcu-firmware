#pragma once
#include <applications.h>

#define RECORD_DESKTOP "desktop"

#ifdef __cplusplus
extern "C" {
#endif

/** Start an app from FLIPPER_APPS by its appid.
 * @param app  pointer to the FlipperInternalApplication struct
 * @return true if the app was started, false if another app is already running
 */
bool desktop_start_app(const FlipperInternalApplication* app);

/** Stop the currently running app, if any. Returns true if an app was stopped,
 * @return true if an app was stopped, false if no app was running
 */
bool desktop_stop_app(void);

/** Get the name of the currently running app.
 * @return the appid of the currently running app, or NULL if no app is running
 */
const char* desktop_get_running_app_name(void);

/** Notify the desktop that an app is running. The app occupies the desktop
 * app slot as if it was started with desktop_start_app(): no other app can
 * be started until desktop_unregister_app() is called, and desktop_stop_app()
 * can be used to request its exit.
 * @param appid application ID (must stay valid until unregister)
 * @param thread the FuriThread instance the app is running in
 * @return true if the app was registered, false if another app is already running
 */
bool desktop_register_app(const char* appid, FuriThread* thread);

/** Notify the desktop that the registered app has exited. Frees the app slot.
 * @param appid application ID (must match the one used in desktop_register_app())
 * @return true if the app was unregistered, false if no app was registered with that ID
 */
bool desktop_unregister_app(const char* appid);

#ifdef __cplusplus
}
#endif
