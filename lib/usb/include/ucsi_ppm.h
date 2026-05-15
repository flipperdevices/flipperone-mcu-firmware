#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "ucsi_ppm_errors.h"
#include "ucsi_ppm_hal.h"
#include "ucsi_ppm_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UCSI_PPM_VERSION_UCSI 0x0300
#define UCSI_PPM_VERSION_PD 0x0300
#define UCSI_PPM_VERSION_TYPEC 0x0200
#define UCSI_PPM_VERSION_BC 0x0000
#define UCSI_PPM_NUM_ALT_MODES 0
#define UCSI_PPM_NUM_CONNECTORS 1

#define UCSI_PPM_API_VERSION_MAJOR 0
#define UCSI_PPM_API_VERSION_MINOR 1
#define UCSI_PPM_API_VERSION_PATCH 0

typedef struct UcsiPpm UcsiPpm;

UcsiPpm* ucsi_ppm_alloc(void);

void ucsi_ppm_free(UcsiPpm* ppm);

UcsiPpmStatus ucsi_ppm_init(UcsiPpm* ppm, const UcsiPpmConfig* config);

UcsiPpmStatus ucsi_ppm_deinit(UcsiPpm* ppm);

UcsiPpmStatus ucsi_ppm_reset(UcsiPpm* ppm);

UcsiPpmStatus ucsi_ppm_tick(UcsiPpm* ppm);

UcsiPpmStatus ucsi_ppm_notify_fusb302_irq(UcsiPpm* ppm);

UcsiPpmStatus ucsi_ppm_notify_power_supply_ready(UcsiPpm* ppm);

UcsiPpmStatus ucsi_ppm_register_read(
    UcsiPpm* ppm,
    uint16_t offset,
    uint16_t length,
    uint8_t* buf);

UcsiPpmStatus ucsi_ppm_register_write(
    UcsiPpm* ppm,
    uint16_t offset,
    uint16_t length,
    const uint8_t* buf);

typedef enum {
    UcsiPpmStateUnattached,
    UcsiPpmStateAttachWait,
    UcsiPpmStateAttachedSrc,
    UcsiPpmStateAttachedSnk,
    UcsiPpmStateErrorRecovery,
    UcsiPpmStateDisabled,
} UcsiPpmConnectorState;

UcsiPpmConnectorState ucsi_ppm_get_connector_state(const UcsiPpm* ppm);

typedef struct {
    bool contract_in_place;
    uint16_t voltage_mv;
    uint16_t current_ma;
    bool is_source;
    bool is_dfp;
} UcsiPpmContractInfo;

UcsiPpmStatus ucsi_ppm_get_contract(const UcsiPpm* ppm, UcsiPpmContractInfo* out);

#ifdef __cplusplus
}
#endif
