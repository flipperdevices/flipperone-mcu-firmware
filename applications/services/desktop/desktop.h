#pragma once
#include <applications.h>

#define RECORD_DESKTOP "desktop"

#ifdef __cplusplus
extern "C" {
#endif

bool desktop_start_app(const FlipperInternalApplication* app);
bool desktop_stop_app(void);

/** Get the name of the currently running app, or NULL if none is running. */
const char* desktop_get_running_app_name(void);

/** Notify the desktop that an app is running. The app occupies the desktop
 * app slot as if it was started with desktop_start_app(): no other app can
 * be started until desktop_unregister_app() is called, and desktop_stop_app()
 * can be used to request its exit.
 * @param appid application ID (must stay valid until unregister)
 * @param thread the FuriThread instance the app is running in
 */
bool desktop_register_app(const char* appid, FuriThread* thread);

/** Notify the desktop that the registered app has exited. Frees the app slot. */
bool desktop_unregister_app(const char* appid);

#ifdef __cplusplus
}
#endif
