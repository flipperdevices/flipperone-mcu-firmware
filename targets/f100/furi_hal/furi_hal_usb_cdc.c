
#include <furi_hal_usb_cdc.h>
#include <furi.h>

#define IF_NUM_MAX CFG_TUD_CDC
static cdc_line_coding_t cdc_config[IF_NUM_MAX] = {0};
static CdcCallbacks* callbacks[IF_NUM_MAX] = {NULL};
static void* cb_ctx[IF_NUM_MAX] = {};
static uint8_t cdc_ctrl_line_state[IF_NUM_MAX] = {};
static bool connected = false;

void tud_cdc_rx_cb(uint8_t itf) {
    furi_check(itf < IF_NUM_MAX);
    if(callbacks[itf] != NULL) {
        if(callbacks[itf]->rx_ep_callback != NULL) {
            callbacks[itf]->rx_ep_callback(cb_ctx[itf]);
        }
    }
}

void tud_cdc_line_state_cb(uint8_t instance, bool dtr, bool rts) {
    furi_check(instance < IF_NUM_MAX);
    // Invoked when cdc when line state changed e.g connected/disconnecte
    // DTR = false is counted as disconnected
    cdc_ctrl_line_state[instance] = (dtr ? 1 : 0) | (rts ? 2 : 0);

    if(callbacks[instance] != NULL) {
        if(callbacks[instance]->ctrl_line_callback != NULL) {
            callbacks[instance]->ctrl_line_callback(cb_ctx[instance], cdc_ctrl_line_state[instance]);
        }
    }
}

void tud_mount_cb(void) {
    connected = true;
    for(uint8_t i = 0; i < IF_NUM_MAX; i++) {
        if(callbacks[i] != NULL) {
            if(callbacks[i]->state_callback != NULL) callbacks[i]->state_callback(cb_ctx[i], 1);
        }
    }
}

void tud_suspend_cb(bool remote_wakeup_en) {
    (void)remote_wakeup_en;
    connected = false;
    for(uint8_t i = 0; i < IF_NUM_MAX; i++) {
        cdc_ctrl_line_state[i] = 0;
        if(callbacks[i] != NULL) {
            if(callbacks[i]->state_callback != NULL) callbacks[i]->state_callback(cb_ctx[i], 0);
        }
    }
}

void tud_cdc_tx_complete_cb(uint8_t itf) {
    furi_check(itf < IF_NUM_MAX);
    if(callbacks[itf] != NULL) {
        if(callbacks[itf]->tx_ep_callback != NULL) {
            callbacks[itf]->tx_ep_callback(cb_ctx[itf]);
        }
    }
}

void furi_hal_cdc_set_callbacks(uint8_t if_num, CdcCallbacks* cb, void* context) {
    furi_check(if_num < IF_NUM_MAX);

    if(callbacks[if_num] != NULL) {
        if(callbacks[if_num]->state_callback != NULL) {
            if(connected == true) callbacks[if_num]->state_callback(cb_ctx[if_num], 0);
        }
    }

    callbacks[if_num] = cb;
    cb_ctx[if_num] = context;

    if(callbacks[if_num] != NULL) {
        if(callbacks[if_num]->state_callback != NULL) {
            if(connected == true) callbacks[if_num]->state_callback(cb_ctx[if_num], 1);
        }
        if(callbacks[if_num]->ctrl_line_callback != NULL) {
            callbacks[if_num]->ctrl_line_callback(cb_ctx[if_num], cdc_ctrl_line_state[if_num]);
        }
    }
}

cdc_line_coding_t* furi_hal_cdc_get_port_settings(uint8_t if_num) {
    //static struct usb_cdc_line_coding line_coding;
    cdc_line_coding_t* line_coding;
    tud_cdc_n_get_line_coding(if_num, line_coding);
    return line_coding;
}

uint8_t furi_hal_cdc_get_ctrl_line_state(uint8_t if_num) {
    return tud_cdc_n_get_line_state(if_num);
}

void furi_hal_cdc_send(uint8_t if_num, uint8_t* buf, uint16_t len) {
    tud_cdc_n_write(if_num, buf, len);
    tud_cdc_n_write_flush(if_num);
}

int32_t furi_hal_cdc_receive(uint8_t if_num, uint8_t* buf, uint16_t max_len) {
    return tud_cdc_n_read(if_num, buf, max_len);
}
