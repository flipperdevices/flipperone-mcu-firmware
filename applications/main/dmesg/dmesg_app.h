#pragma once

#include <furi_hal_resources.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RECORD_DMESG_APP "dmesg_app"

typedef struct DmesgApp DmesgApp;

bool dmesg_app_get_log_data(DmesgApp* instance, uint8_t* data, size_t* size);
void dmesg_app_update_read_index(DmesgApp* instance);

#ifdef __cplusplus
}
#endif
