#pragma once
#include <applications.h>

#define RECORD_DESKTOP "desktop"

#ifdef __cplusplus
extern "C" {
#endif

bool desktop_start_app(const FlipperInternalApplication* app);

#ifdef __cplusplus
}
#endif
