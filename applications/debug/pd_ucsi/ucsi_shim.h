#pragma once

// Thin helpers for driving UcsiPpm from C code, without an external OPM.
// Each helper packs a CONTROL register, fires it through the public regfile
// API, waits for Command Completed, optionally copies MESSAGE_IN out, then
// ACKs the CCI. Returns false on timeout or CCI Error.
//
// All helpers block for up to UCSI_SHIM_CMD_TIMEOUT_MS and assume the PPM
// has been initialised with a valid time_ms hook (used by ucsi_wait_cc).
//
// For bring-up only — production OPM logic should drive the regfile directly.

#include <stdint.h>
#include <stdbool.h>

#include <ucsi_ppm.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UCSI_SHIM_CMD_TIMEOUT_MS 500u

// Enables every defined notification bit (CSC + Command Completed).
bool ucsi_shim_set_notification_enable(UcsiPpm* ppm, uint32_t mask);

// Picks one of Rp-only / Rd-only / DRP / Disabled for the connector role.
bool ucsi_shim_set_ccom(UcsiPpm* ppm, uint8_t cc_mode_bits);

// SET_UOR: bit 0 = initiate swap to DFP, bit 1 = swap to UFP, bit 2 = accept.
bool ucsi_shim_set_uor(UcsiPpm* ppm, uint8_t role_bits);

// SET_PDR: bit 0 = initiate swap to Source, bit 1 = Sink, bit 2 = accept.
bool ucsi_shim_set_pdr(UcsiPpm* ppm, uint8_t role_bits);

// SET_POWER_LEVEL on the sink side. op_current_ma is rounded to 50 mA units
// (UCSI granularity). 0 = "PPM-decides" → renegotiate to advertised max.
bool ucsi_shim_set_power_level_sink(UcsiPpm* ppm, uint16_t op_current_ma);

// Reads MESSAGE_IN payloads for the corresponding queries. Caller-provided
// buffers must hold at least the documented sizes (commands.md §2.x).
bool ucsi_shim_get_capability(UcsiPpm* ppm, uint8_t out[16]);
bool ucsi_shim_get_connector_capability(UcsiPpm* ppm, uint8_t out[4]);
bool ucsi_shim_get_connector_status(UcsiPpm* ppm, uint8_t out[19]);
bool ucsi_shim_get_error_status(UcsiPpm* ppm, uint8_t out[16]);

// Reads up to num PDOs starting at pdo_offset. partner=true → partner caps,
// false → our own caps. source=true → Source PDOs, false → Sink PDOs.
// Returns count of PDOs copied (0 on error).
uint8_t ucsi_shim_get_pdos(UcsiPpm* ppm, bool partner, uint8_t pdo_offset, uint8_t num, bool source, uint32_t out_pdos[4]);

// Pretty-prints the current connector status via FURI_LOG_I — convenient one
// liner for bring-up monitoring loops.
void ucsi_shim_log_status(UcsiPpm* ppm);

// Same, but only logs when the Connector Status Change bitmap is non-zero
// (PE/TC raise CSC bits on any meaningful transition; reading clears them).
// Use in periodic poll loops to keep the log quiet when nothing happens.
// Returns true if a line was printed.
bool ucsi_shim_log_status_if_changed(UcsiPpm* ppm);

#ifdef __cplusplus
}
#endif
