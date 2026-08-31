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

/** Check if the VCI DC-DC is initialized
 * @return bool - whether the DC-DC is initialized
 */
bool furi_bsp_vci_is_initialized(void);

/** Set the display VCI voltage
 * @param voltage - voltage in volts
 */
void furi_bsp_vci_set_voltage(float voltage);

/** Get the display VCI voltage
 * @return float - voltage in volts, 0.0f if the DC-DC is not initialized
 */
float furi_bsp_vci_get_voltage(void);

#ifdef __cplusplus
}
#endif
