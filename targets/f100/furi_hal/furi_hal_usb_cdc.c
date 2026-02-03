
#include <furi_hal_usb_cdc.h>
#include <furi.h>

#define FURI_HAL_USB_CDC_TIMEOUT_US          500000
#define FURI_HAL_USB_CDC_DEADLOCK_TIMEOUT_MS 1000

FuriMutex* furi_hal_usb_cdc_mutex = NULL;

#define IF_NUM_MAX CFG_TUD_CDC
static cdc_line_coding_t cdc_config[IF_NUM_MAX] = {0};
static CdcCallbacks* callbacks[IF_NUM_MAX] = {NULL};
static void* cb_ctx[IF_NUM_MAX] = {};
static uint8_t cdc_ctrl_line_state[IF_NUM_MAX] = {};
static bool connected = false;

void furi_hal_usb_cdc_init() {
    furi_check(furi_hal_usb_cdc_mutex == NULL);
    furi_hal_usb_cdc_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
}

void furi_hal_usb_cdc_deinit() {
    furi_check(furi_hal_usb_cdc_mutex != NULL);
    furi_mutex_free(furi_hal_usb_cdc_mutex);
    furi_hal_usb_cdc_mutex = NULL;
}

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
    FuriStatus status = furi_mutex_acquire(furi_hal_usb_cdc_mutex, FURI_HAL_USB_CDC_DEADLOCK_TIMEOUT_MS);

    if(status == FuriStatusErrorTimeout) {
        return;
    }

    furi_check(status == FuriStatusOk);

    static uint64_t last_avail_time;
    if(tud_ready()) {
        for(int i = 0; i < len;) {
            int n = len - i;
            int avail = (int)tud_cdc_n_write_available(if_num);
            if(n > avail) n = avail;
            if(n) {
                int n2 = (int)tud_cdc_n_write(if_num, buf + i, (uint32_t)n);
                tud_cdc_n_write_flush(if_num);
                i += n2;
                last_avail_time = time_us_64();
            } else {
                tud_cdc_n_write_flush(if_num);
                if(!tud_ready() || (!tud_cdc_n_write_available(if_num) && time_us_64() > last_avail_time + FURI_HAL_USB_CDC_TIMEOUT_US)) {
                    break;
                }
            }
        }
    } else {
        // reset our timeout
        last_avail_time = 0;
    }

    furi_check(furi_mutex_release(furi_hal_usb_cdc_mutex) == FuriStatusOk);
}

int32_t furi_hal_cdc_receive(uint8_t if_num, uint8_t* buf, uint16_t length) {
    // note we perform this check outside the lock, to try and prevent possible deadlock conditions
    // with printf in IRQs (which we will escape through timeouts elsewhere, but that would be less graceful).
    //
    // these are just checks of state, so we can call them while not holding the lock.
    // they may be wrong, but only if we are in the middle of a tud_task call, in which case at worst
    // we will mistakenly think we have data available when we do not (we will check again), or
    // tud_task will complete running and we will check the right values the next time.
    //
    int rc = PICO_ERROR_NO_DATA;
    if(tud_ready() && tud_cdc_n_available(if_num)) {
        FuriStatus status = furi_mutex_acquire(furi_hal_usb_cdc_mutex, FURI_HAL_USB_CDC_DEADLOCK_TIMEOUT_MS);

        if(status == FuriStatusErrorTimeout) {
            return PICO_ERROR_NO_DATA;
        }
        furi_check(status == FuriStatusOk);

        if(tud_ready() && tud_cdc_n_available(if_num)) {
            int count = (int)tud_cdc_n_read(if_num, buf, (uint32_t)length);
            rc = count ? count : PICO_ERROR_NO_DATA;
        }
        furi_check(furi_mutex_release(furi_hal_usb_cdc_mutex) == FuriStatusOk);
    }
    return rc;
}
