#pragma once
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** The self check screen: which devices the MCU found at boot, the serial,
 * hardware and firmware versions, on a black background. Shared by the self
 * check app and the host app, which shows it while the host starts. */
typedef struct SelfCheckScreen SelfCheckScreen;

/** Check the devices found at MCU boot and prepare the texts */
SelfCheckScreen* self_check_screen_alloc(void);

void self_check_screen_free(SelfCheckScreen* instance);

/** True when every device was found */
bool self_check_screen_is_ok(const SelfCheckScreen* instance);

/** Lay the screen out as a Clay element that fills its parent, under this
 * title. The texts stay owned by the instance and the caller, keep them alive
 * while rendering. */
void self_check_screen_layout(const SelfCheckScreen* instance, const char* title);

#ifdef __cplusplus
}
#endif
