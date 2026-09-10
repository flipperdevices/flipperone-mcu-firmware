#pragma once
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Init the TPS62868x DC-DC that powers the display VCI rail
 * and set the default voltage
 */
void furi_bsp_vci_init(void);

/** Deinit the VCI DC-DC, turning the rail off */
void furi_bsp_vci_deinit(void);

#ifdef __cplusplus
}
#endif
