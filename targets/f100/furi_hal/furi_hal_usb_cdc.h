#pragma once

#include <stdint.h>
#include <tusb.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    void (*tx_ep_callback)(void* context);
    void (*rx_ep_callback)(void* context);
    void (*state_callback)(void* context, uint8_t state);
    void (*ctrl_line_callback)(void* context, uint8_t state);
    void (*config_callback)(void* context, cdc_line_coding_t* config);
} CdcCallbacks;

void furi_hal_cdc_set_callbacks(uint8_t if_num, CdcCallbacks* cb, void* context);

cdc_line_coding_t* furi_hal_cdc_get_port_settings(uint8_t if_num);

uint8_t furi_hal_cdc_get_ctrl_line_state(uint8_t if_num);

void furi_hal_cdc_send(uint8_t if_num, uint8_t* buf, uint16_t len);

int32_t furi_hal_cdc_receive(uint8_t if_num, uint8_t* buf, uint16_t max_len);

#ifdef __cplusplus
}
#endif
