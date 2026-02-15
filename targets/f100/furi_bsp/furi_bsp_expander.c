#include <furi_hal_i2c_config.h>
#include <drivers/tca6416a/tca6416a.h>
#include <stdio.h>

#include "furi_bsp_expander.h"

typedef struct {
    Tca6416a* handle;
    FuriCallback callback;
} ExpanderControl;

typedef struct {
    Tca6416a* handle;
    FuriThreadId thread_id;
    InputExpMain input_mask_old;
    FuriBsbControlExpanderMain control_state;

    FuriCallback gpio_5v0_flt_callback;
    void* gpio_5v0_flt_callback_context;
    FuriCallback gpio_3v3_flt_callback;
    void* gpio_3v3_flt_callback_context;
    FuriCallback bq25798_callback;
    void* bq25798_callback_context;
    FuriCallback fusb302_callback;
    void* fusb302_callback_context;
    FuriCallback mux_vconn_fault_callback;
    void* mux_vconn_fault_callback_context;
    FuriCallback type_c_up_sw_pg_callback;
    void* type_c_up_sw_pg_callback_context;
    FuriCallback type_a_up_sw_pg_callback;
    void* type_a_up_sw_pg_callback_context;
    FuriCallback expander7_callback;
    void* expander7_callback_context;
} ExpanderMain;

#define TAG "Expander"

#define EXPANDER_MAIN_THREAD_FLAG_ISR 0x00000001

#define EXPANDER_DEBUG_ENABLE

#ifdef EXPANDER_DEBUG_ENABLE
#define EXPANDER_DEBUG(...) FURI_LOG_I(TAG, __VA_ARGS__)
#else
#define EXPANDER_DEBUG(...)
#endif

static ExpanderControl* expander_control = NULL;
static ExpanderMain* expander_main = NULL;

static void furi_bsp_expander_control_init(void) {
    furi_check(expander_control == NULL);
    expander_control = malloc(sizeof(ExpanderControl));
    expander_control->handle = tca6416a_init(&furi_hal_i2c_handle_internal, &gpio_expander_reset, &gpio_expander_int, TCA6416A_ADDRESS_A0);
    tca6416a_write_mode(expander_control->handle, InputKeyMask);
}

static __isr __not_in_flash_func(void) furi_bsp_expander_main_interrupt_handler(void* ctx) {
    ExpanderMain* instance = (ExpanderMain*)ctx;
    furi_thread_flags_set(instance->thread_id, EXPANDER_MAIN_THREAD_FLAG_ISR);
}

static int32_t subghz_worker_thread_callback(void* context) {
    ExpanderMain* instance = context;

    while(1) {
        furi_thread_flags_wait(EXPANDER_MAIN_THREAD_FLAG_ISR, FuriFlagWaitAny, FuriWaitForever);
        InputExpMain input = ~tca6416a_read_input(instance->handle) & InputExpMainInputMask;
        EXPANDER_DEBUG("Expander Main Input: 0x%02X", input);
        InputExpMain changed = (input ^ instance->input_mask_old) & input;
        instance->input_mask_old = input;

        if(changed & InputExpMainGpio5v0Flt) {
            EXPANDER_DEBUG("GPIO 5V0 Fault Detected");
            if(instance->gpio_5v0_flt_callback) {
                instance->gpio_5v0_flt_callback(instance->gpio_5v0_flt_callback_context);
            }
        }
        if(changed & InputExpMainGpio3v3Flt) {
            EXPANDER_DEBUG("GPIO 3V3 Fault Detected");
            if(instance->gpio_3v3_flt_callback) {
                instance->gpio_3v3_flt_callback(instance->gpio_3v3_flt_callback_context);
            }
        }
        if(changed & InputExpMainBq25798Int) {
            EXPANDER_DEBUG("BQ25798 Interrupt Detected");
            if(instance->bq25798_callback) {
                instance->bq25798_callback(instance->bq25798_callback_context);
            }
        }
        if(changed & InputExpMainFusb302Int) {
            EXPANDER_DEBUG("FUSB302 Interrupt Detected");
            if(instance->fusb302_callback) {
                instance->fusb302_callback(instance->fusb302_callback_context);
            }
        }
        if(changed & InputExpMainMuxVconnFault) {
            EXPANDER_DEBUG("MUX VCON Fault Detected");
            if(instance->mux_vconn_fault_callback) {
                instance->mux_vconn_fault_callback(instance->mux_vconn_fault_callback_context);
            }
        }
        if(changed & InputExpMainTypeCUpSwPg) {
            EXPANDER_DEBUG("Type-C Up SW PG Detected");
            if(instance->type_c_up_sw_pg_callback) {
                instance->type_c_up_sw_pg_callback(instance->type_c_up_sw_pg_callback_context);
            }
        }
        if(changed & InputExpMainTypeAUpSwPg) {
            EXPANDER_DEBUG("Type-A Up SW PG Detected");
            if(instance->type_a_up_sw_pg_callback) {
                instance->type_a_up_sw_pg_callback(instance->type_a_up_sw_pg_callback_context);
            }
        }
        if(changed & InputExpMainExpander7) {
            EXPANDER_DEBUG("Expander 7 Interrupt Detected");
            if(instance->expander7_callback) {
                instance->expander7_callback(instance->expander7_callback_context);
            }
        }
    }
    furi_crash();
    return 0;
}

static void furi_bsp_expander_main_init(void) {
    furi_check(expander_main == NULL);
    expander_main = malloc(sizeof(ExpanderMain));
    expander_main->handle = tca6416a_init(&furi_hal_i2c_handle_external, &gpio_main_board_reset, &gpio_main_expander_int, TCA6416A_ADDRESS_A0);
    tca6416a_set_input_callback(expander_main->handle, furi_bsp_expander_main_interrupt_handler, expander_main);
    expander_main->control_state = FuriBsbControlExpanderMainOn;
    // Todo: Errata lay the I2C line
    furi_bsp_expander_main_write_output(OutputExpMainVcc5v0DevS0En);
    tca6416a_write_mode(expander_main->handle, InputExpMainInputMask);

    expander_main->input_mask_old = ~tca6416a_read_input(expander_main->handle) & InputExpMainInputMask;
    expander_main->thread_id = furi_thread_alloc_ex("ExpanderMainWorker", 1024, subghz_worker_thread_callback, expander_main);
    furi_thread_start(expander_main->thread_id);
}

void furi_bsp_main_reset(void) {
    furi_check(expander_main != NULL);
    tca6416a_deinit(expander_main->handle);

    furi_hal_gpio_write_open_drain(&gpio_main_board_reset, false);
    furi_delay_ms(10);
    furi_hal_gpio_write_open_drain(&gpio_main_board_reset, true);
    furi_delay_ms(10);

    tca6416a_init(&furi_hal_i2c_handle_external, &gpio_main_board_reset, &gpio_main_expander_int, TCA6416A_ADDRESS_A0);
    tca6416a_set_input_callback(expander_main->handle, furi_bsp_expander_main_interrupt_handler, expander_main);
    expander_main->control_state = FuriBsbControlExpanderMainOn;
    // Todo: Errata lay the I2C line
    furi_bsp_expander_main_write_output(OutputExpMainVcc5v0DevS0En);
    tca6416a_write_mode(expander_main->handle, InputExpMainInputMask);
}

void furi_bsp_expander_init(void) {
    furi_check(expander_control == NULL);
    furi_check(expander_main == NULL);
    furi_bsp_expander_control_init();
    furi_bsp_expander_main_init();
}

InputKey furi_bsp_expander_control_read_buttons(void) {
    furi_assert(expander_control != NULL);
    return tca6416a_read_input(expander_control->handle) & InputKeyMask;
}

void furi_bsp_expander_control_attach_buttons_callback(FuriCallback callback, void* context) {
    furi_check(callback != NULL);
    furi_check(expander_control != NULL);
    furi_check(expander_control->callback == NULL);
    tca6416a_set_input_callback(expander_control->handle, callback, context);
}

void furi_bsp_expander_control_led_power(StatusLedPower led_mask) {
    furi_check(expander_control != NULL);
    tca6416a_write_output(expander_control->handle, led_mask & StatusLedPowerMask);
}

void furi_bsp_expander_main_write_output(OutputExpMain output_mask) {
    furi_check(expander_main != NULL);
    tca6416a_write_output(expander_main->handle, output_mask & OutputExpMainMask);
}

OutputExpMain furi_bsp_expander_main_read_output(void) {
    furi_check(expander_main != NULL);
    return tca6416a_read_input(expander_main->handle) & OutputExpMainMask;
}

InputExpMain furi_bsp_expander_main_read_input(void) {
    furi_assert(expander_main != NULL);
    return tca6416a_read_input(expander_main->handle) & InputExpMainInputMask;
}

void furi_bsp_expander_main_set_control(FuriBsbControlExpanderMain control) {
    furi_check(expander_main != NULL);
    if(control == expander_main->control_state) {
        return;
    }
    if(control == FuriBsbControlExpanderMainOn) {
        tca6416a_set_input_callback(expander_main->handle, furi_bsp_expander_main_interrupt_handler, expander_main);
        expander_main->control_state = FuriBsbControlExpanderMainOn;
    } else {
        tca6416a_set_input_callback(expander_main->handle, NULL, NULL);
        expander_main->control_state = FuriBsbControlExpanderMainOff;
    }
}

FuriBsbControlExpanderMain furi_bsp_expander_main_get_control_state(void) {
    furi_check(expander_main != NULL);
    return expander_main->control_state;
}

void furi_bsp_expander_main_attach_gpio_5v0_flt_callback(FuriCallback callback, void* context) {
    furi_check(callback != NULL);
    furi_check(expander_main != NULL);
    furi_check(expander_main->gpio_5v0_flt_callback == NULL);
    expander_main->gpio_5v0_flt_callback = callback;
    expander_main->gpio_5v0_flt_callback_context = context;
}

void furi_bsp_expander_main_attach_gpio_3v3_flt_callback(FuriCallback callback, void* context) {
    furi_check(callback != NULL);
    furi_check(expander_main != NULL);
    furi_check(expander_main->gpio_3v3_flt_callback == NULL);
    expander_main->gpio_3v3_flt_callback = callback;
    expander_main->gpio_3v3_flt_callback_context = context;
}

void furi_bsp_expander_main_attach_bq25798_callback(FuriCallback callback, void* context) {
    furi_check(callback != NULL);
    furi_check(expander_main != NULL);
    furi_check(expander_main->bq25798_callback == NULL);
    expander_main->bq25798_callback = callback;
    expander_main->bq25798_callback_context = context;
}

void furi_bsp_expander_main_attach_fusb302_callback(FuriCallback callback, void* context) {
    furi_check(callback != NULL);
    furi_check(expander_main != NULL);
    furi_check(expander_main->fusb302_callback == NULL);
    expander_main->fusb302_callback = callback;
    expander_main->fusb302_callback_context = context;
}

void furi_bsp_expander_main_attach_mux_vconn_fault_callback(FuriCallback callback, void* context) {
    furi_check(callback != NULL);
    furi_check(expander_main != NULL);
    furi_check(expander_main->mux_vconn_fault_callback == NULL);
    expander_main->mux_vconn_fault_callback = callback;
    expander_main->mux_vconn_fault_callback_context = context;
}

void furi_bsp_expander_main_attach_type_c_up_sw_pg_callback(FuriCallback callback, void* context) {
    furi_check(callback != NULL);
    furi_check(expander_main != NULL);
    furi_check(expander_main->type_c_up_sw_pg_callback == NULL);
    expander_main->type_c_up_sw_pg_callback = callback;
    expander_main->type_c_up_sw_pg_callback_context = context;
}

void furi_bsp_expander_main_attach_type_a_up_sw_pg_callback(FuriCallback callback, void* context) {
    furi_check(callback != NULL);
    furi_check(expander_main != NULL);
    furi_check(expander_main->type_a_up_sw_pg_callback == NULL);
    expander_main->type_a_up_sw_pg_callback = callback;
    expander_main->type_a_up_sw_pg_callback_context = context;
}

void furi_bsp_expander_main_attach_expander7_callback(FuriCallback callback, void* context) {
    furi_check(callback != NULL);
    furi_check(expander_main != NULL);
    furi_check(expander_main->expander7_callback == NULL);
    expander_main->expander7_callback = callback;
    expander_main->expander7_callback_context = context;
}
