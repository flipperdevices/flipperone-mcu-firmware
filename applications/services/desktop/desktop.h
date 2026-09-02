#pragma once
#include <applications.h>

#define RECORD_DESKTOP "desktop"

#ifdef __cplusplus
extern "C" {
#endif

/** Start an application by FLIPPER_APPS entry.
 * @param app pointer to the FlipperInternalApplication struct
 * @return true if the app was started, false if another app is already running
 */
bool desktop_start_app(const FlipperInternalApplication* app);

/** Stop the currently running app, if any.
 * @return true if an app was running and an exit was requested, false if no app was running
 */
bool desktop_stop_app(void);

/** Get the name of the currently running app (human-readable; equals the
 * appid for apps registered via desktop_register_app()).
 * @return the name of the currently running app, or NULL if no app is running
 */
const char* desktop_get_running_app_name(void);

/** Get the appid of the currently running app.
 * @return the appid of the currently running app, or NULL if no app is running
 */
const char* desktop_get_running_app_id(void);

/** Notify the desktop that an app is running. The app occupies the desktop
 * app slot as if it was started with desktop_start_app(): no other app can
 * be started until desktop_unregister_app() is called, and desktop_stop_app()
 * can be used to request its exit.
 * The app must call desktop_unregister_app() before its thread returns; the
 * desktop joins and frees the thread asynchronously after that.
 * @param appid application ID: must match an entry in FLIPPER_APPS or
 *              FLIPPER_AUTORUN_APPS and the appid of the given thread
 * @param thread the FuriThread instance the app is running in
 * @return true if the app was registered, false if another app is already
 *         running, the appid is unknown, or it does not match the thread
 */
bool desktop_register_app(const char* appid, FuriThread* thread);

/** Notify the desktop that the registered app has exited. Frees the app slot.
 * @param appid application ID (must match the one used in desktop_register_app())
 * @return true if the app was unregistered, false if no external app with
 *         that appid is registered
 */
bool desktop_unregister_app(const char* appid);

#ifdef __cplusplus
}
#endif
