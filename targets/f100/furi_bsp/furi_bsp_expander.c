#include <furi_hal_i2c_config.h>
#include <drivers/pcal6416/pcal6416.h>
#include <stdio.h>

#include <furi_bsp_expander.h>

#define TAG "BspExpander"

typedef struct {
    Pcal6416* handle;
    FuriCallback callback;
} ExpanderControl;

typedef struct {
    FuriCallback callback;
    void* context;
} ExpanderCallbackStorage;

typedef struct {
    Pcal6416* handle;
    FuriThreadId thread_id;
    InputExpMain input_mask_old;
    FuriBspControlExpanderMain control_state;

    ExpanderCallbackStorage gpio_5v0_flt;
    ExpanderCallbackStorage gpio_3v3_flt;
    ExpanderCallbackStorage bq2579x;
    ExpanderCallbackStorage fusb302;
    ExpanderCallbackStorage mux_vconn_fault;
    ExpanderCallbackStorage type_c_up_sw_pg;
    ExpanderCallbackStorage type_a_up_sw_pg;
    ExpanderCallbackStorage expander7;
} ExpanderMain;

#define EXPANDER_MAIN_THREAD_FLAG_ISR 0x00000001

// #define EXPANDER_DEBUG_ENABLE

#ifdef EXPANDER_DEBUG_ENABLE
#define EXPANDER_DEBUG(...) FURI_LOG_I(TAG, __VA_ARGS__)
#else
#define EXPANDER_DEBUG(...)
#endif

static ExpanderControl* expander_control = NULL;
static ExpanderMain* expander_main = NULL;

static void furi_bsp_show_error_message_main_expander(void) {
    FURI_LOG_E(TAG, "Not initialized Main Expander");
}

static void furi_bsp_show_error_message_control_expander(void) {
    FURI_LOG_E(TAG, "Not initialized Control Expander");
}

static void furi_bsp_set_callback(ExpanderCallbackStorage* storage, FuriCallback callback, void* context) {
    FURI_CRITICAL_ENTER();
    storage->callback = callback;
    storage->context = context;
    FURI_CRITICAL_EXIT();
}

static void furi_bsp_expander_control_init(void) {
    furi_check(expander_control == NULL);

    expander_control = malloc(sizeof(ExpanderControl));
    expander_control->handle = pcal6416_init(&furi_hal_i2c_handle_control, &gpio_expander_reset, &gpio_expander_int, PCAL6416_ADDRESS_A0);
    if(expander_control->handle) {
        FURI_LOG_I(TAG, "Initializing Control Expander");
        pcal6416_write_output(expander_control->handle, 0x0000); // All outputs low by default
        pcal6416_write_mode(expander_control->handle, InputKeyMask);
    } else {
        FURI_LOG_E(TAG, "Failed to initialize Control Expander");
    }
}

static __isr __not_in_flash_func(void) furi_bsp_expander_main_interrupt_handler(void* ctx) {
    ExpanderMain* instance = (ExpanderMain*)ctx;
    furi_thread_flags_set(instance->thread_id, EXPANDER_MAIN_THREAD_FLAG_ISR);
}

static int32_t furi_bsp_expander_callback_thread(void* context) {
    ExpanderMain* instance = context;

    while(1) {
        furi_thread_flags_wait(EXPANDER_MAIN_THREAD_FLAG_ISR, FuriFlagWaitAny, FuriWaitForever);

        // Trigger on interrupts that changed and transitioned from high to low (active low)
        InputExpMain input = ~pcal6416_read_input(instance->handle) & InputExpMainInputMask;
        EXPANDER_DEBUG("Expander Main Input: 0x%02X", input);
        InputExpMain changed = (input ^ instance->input_mask_old) & input;
        instance->input_mask_old = input;

        if(changed & InputExpMainGpio5v0Flt) {
            EXPANDER_DEBUG("GPIO 5V0 Fault Detected");
            if(instance->gpio_5v0_flt.callback) {
                instance->gpio_5v0_flt.callback(instance->gpio_5v0_flt.context);
            }
        }
        if(changed & InputExpMainGpio3v3Flt) {
            EXPANDER_DEBUG("GPIO 3V3 Fault Detected");
            if(instance->gpio_3v3_flt.callback) {
                instance->gpio_3v3_flt.callback(instance->gpio_3v3_flt.context);
            }
        }
        if(changed & InputExpMainBq2579xInt) {
            EXPANDER_DEBUG("bq2579x Interrupt Detected");
            if(instance->bq2579x.callback) {
                instance->bq2579x.callback(instance->bq2579x.context);
            }
        }
        if(changed & InputExpMainFusb302Int) {
            EXPANDER_DEBUG("FUSB302 Interrupt Detected");
            if(instance->fusb302.callback) {
                instance->fusb302.callback(instance->fusb302.context);
            }
        }
        if(changed & InputExpMainMuxVconnFault) {
            EXPANDER_DEBUG("MUX VCON Fault Detected");
            if(instance->mux_vconn_fault.callback) {
                instance->mux_vconn_fault.callback(instance->mux_vconn_fault.context);
            }
        }
        if(changed & InputExpMainTypeCUpSwPg) {
            EXPANDER_DEBUG("Type-C Up SW PG Detected");
            if(instance->type_c_up_sw_pg.callback) {
                instance->type_c_up_sw_pg.callback(instance->type_c_up_sw_pg.context);
            }
        }
        if(changed & InputExpMainTypeAUpSwPg) {
            EXPANDER_DEBUG("Type-A Up SW PG Detected");
            if(instance->type_a_up_sw_pg.callback) {
                instance->type_a_up_sw_pg.callback(instance->type_a_up_sw_pg.context);
            }
        }
        if(changed & InputExpMainExpander7) {
            EXPANDER_DEBUG("Expander 7 Interrupt Detected");
            if(instance->expander7.callback) {
                instance->expander7.callback(instance->expander7.context);
            }
        }
    }
    furi_crash();
    return 0;
}

static void furi_bsp_expander_main_init(void) {
    furi_check(expander_main == NULL);

    expander_main = malloc(sizeof(ExpanderMain));
    expander_main->handle = pcal6416_init(&furi_hal_i2c_handle_main, &gpio_main_board_reset, &gpio_main_expander_int, PCAL6416_ADDRESS_A0);

    if(expander_main->handle) {
        FURI_LOG_I(TAG, "Initializing Main Expander");
        pcal6416_set_input_callback(expander_main->handle, furi_bsp_expander_main_interrupt_handler, expander_main);
        expander_main->control_state = FuriBspControlExpanderMainMcu;
        // Todo: Errata lay the I2C line
        uint32_t output_mask = furi_bsp_expander_main_read_output();
        furi_bsp_expander_main_write_output(output_mask | OutputExpMainVcc5v0DevS0En);
        pcal6416_write_mode(expander_main->handle, InputExpMainInputMask);

        expander_main->input_mask_old = ~pcal6416_read_input(expander_main->handle) & InputExpMainInputMask;
        expander_main->thread_id = furi_thread_alloc_ex("ExpanderMainWorker", 1024, furi_bsp_expander_callback_thread, expander_main);
        furi_thread_start(expander_main->thread_id);
    } else {
        FURI_LOG_E(TAG, "Failed to initialize Main Expander");
    }
}

void furi_bsp_main_reset(void) {
    furi_check(expander_main != NULL);
    if(expander_main->handle) {
        pcal6416_deinit(expander_main->handle);

        furi_hal_gpio_write_open_drain(&gpio_main_board_reset, false);
        furi_delay_ms(50);
        furi_hal_gpio_write_open_drain(&gpio_main_board_reset, true);
        furi_delay_ms(10);

        pcal6416_init(&furi_hal_i2c_handle_main, &gpio_main_board_reset, &gpio_main_expander_int, PCAL6416_ADDRESS_A0);
        pcal6416_set_input_callback(expander_main->handle, furi_bsp_expander_main_interrupt_handler, expander_main);
        expander_main->control_state = FuriBspControlExpanderMainMcu;
        // Todo: Errata lay the I2C line
        furi_bsp_expander_main_write_output(OutputExpMainVcc5v0DevS0En);
        pcal6416_write_mode(expander_main->handle, InputExpMainInputMask);
    } else {
        furi_bsp_show_error_message_main_expander();
    }
}

void furi_bsp_expander_init(void) {
    furi_check(expander_control == NULL);
    furi_check(expander_main == NULL);
    furi_bsp_expander_control_init();
    furi_bsp_expander_main_init();
}

bool furi_bsp_expander_is_initialized(FuriBspDevice* device) {
    bool control_initialized = expander_control != NULL && expander_control->handle != NULL;
    bool main_initialized = expander_main != NULL && expander_main->handle != NULL;

    if(device) {
        *device = 0;
        if(control_initialized) {
            *device |= FuriBspDeviceExpanderControl;
        }
        if(main_initialized) {
            *device |= FuriBspDeviceExpanderMain;
        }
    }

    return control_initialized && main_initialized;
}

uint16_t furi_bsp_expander_control_read_buttons(void) {
    furi_assert(expander_control != NULL);
    if(expander_control->handle) {
        return pcal6416_read_input(expander_control->handle) & InputKeyMask;
    } else {
        furi_bsp_show_error_message_control_expander();
        return 0;
    }
}

void furi_bsp_expander_control_attach_buttons_callback(FuriCallback callback, void* context) {
    furi_check(callback != NULL);
    if(expander_control->handle) {
        furi_check(expander_control->callback == NULL);
        pcal6416_set_input_callback(expander_control->handle, callback, context);
    } else {
        furi_bsp_show_error_message_control_expander();
    }
}

void furi_bsp_expander_control_led_power(uint16_t led_mask) {
    furi_check(expander_control != NULL);
    if(expander_control->handle) {
        pcal6416_write_output(expander_control->handle, led_mask & StatusLedPowerMask);
    } else {
        furi_bsp_show_error_message_control_expander();
    }
}

void furi_bsp_expander_main_write_output(uint16_t output_mask) {
    furi_check(expander_main != NULL);
    if(expander_main->handle) {
        pcal6416_write_output(expander_main->handle, output_mask & OutputExpMainMask);
    } else {
        furi_bsp_show_error_message_main_expander();
    }
}

uint16_t furi_bsp_expander_main_read_output(void) {
    furi_check(expander_main != NULL);
    if(expander_main->handle) {
        return pcal6416_read_input(expander_main->handle) & OutputExpMainMask;
    } else {
        furi_bsp_show_error_message_main_expander();
        return 0;
    }
}

uint16_t furi_bsp_expander_main_read_input(void) {
    furi_assert(expander_main != NULL);
    if(expander_main->handle) {
        return pcal6416_read_input(expander_main->handle) & InputExpMainInputMask;
    } else {
        furi_bsp_show_error_message_main_expander();
        return 0;
    }
}

void furi_bsp_expander_main_set_control(FuriBspControlExpanderMain control) {
    furi_check(expander_main != NULL);
    if(expander_main->handle) {
        if(control == expander_main->control_state) {
            return;
        }
        if(control == FuriBspControlExpanderMainMcu) {
            pcal6416_set_input_callback(expander_main->handle, furi_bsp_expander_main_interrupt_handler, expander_main);
            expander_main->control_state = FuriBspControlExpanderMainMcu;
        } else {
            pcal6416_set_input_callback(expander_main->handle, NULL, NULL);
            expander_main->control_state = FuriBspControlExpanderMainCpu;
        }
    } else {
        furi_bsp_show_error_message_main_expander();
    }
}

FuriBspControlExpanderMain furi_bsp_expander_main_get_control_state(void) {
    furi_check(expander_main != NULL);
    if(expander_main->handle) {
        return expander_main->control_state;
    } else {
        furi_bsp_show_error_message_main_expander();
        return FuriBspControlExpanderMainCpu; // Return a default value or handle error appropriately
    }
}

void furi_bsp_expander_main_attach_gpio_5v0_flt_callback(FuriCallback callback, void* context) {
    furi_check(callback != NULL);
    furi_check(expander_main != NULL);
    if(expander_main->handle) {
        furi_check(expander_main->gpio_5v0_flt.callback == NULL);
        furi_bsp_set_callback(&expander_main->gpio_5v0_flt, callback, context);
    } else {
        furi_bsp_show_error_message_main_expander();
    }
}

void furi_bsp_expander_main_attach_gpio_3v3_flt_callback(FuriCallback callback, void* context) {
    furi_check(callback != NULL);
    furi_check(expander_main != NULL);
    if(expander_main->handle) {
        furi_check(expander_main->gpio_3v3_flt.callback == NULL);
        furi_bsp_set_callback(&expander_main->gpio_3v3_flt, callback, context);
    } else {
        furi_bsp_show_error_message_main_expander();
    }
}

void furi_bsp_expander_main_attach_bq2579x_callback(FuriCallback callback, void* context) {
    furi_check(callback != NULL);
    furi_check(expander_main != NULL);
    if(expander_main->handle) {
        furi_check(expander_main->bq2579x.callback == NULL);
        furi_bsp_set_callback(&expander_main->bq2579x, callback, context);
    } else {
        furi_bsp_show_error_message_main_expander();
    }
}

void furi_bsp_expander_main_attach_fusb302_callback(FuriCallback callback, void* context) {
    furi_check(callback != NULL);
    furi_check(expander_main != NULL);
    if(expander_main->handle) {
        furi_check(expander_main->fusb302.callback == NULL);
        furi_bsp_set_callback(&expander_main->fusb302, callback, context);
    } else {
        furi_bsp_show_error_message_main_expander();
    }
}

void furi_bsp_expander_main_attach_mux_vconn_fault_callback(FuriCallback callback, void* context) {
    furi_check(callback != NULL);
    furi_check(expander_main != NULL);
    if(expander_main->handle) {
        furi_check(expander_main->mux_vconn_fault.callback == NULL);
        furi_bsp_set_callback(&expander_main->mux_vconn_fault, callback, context);
    } else {
        furi_bsp_show_error_message_main_expander();
    }
}

void furi_bsp_expander_main_attach_type_c_up_sw_pg_callback(FuriCallback callback, void* context) {
    furi_check(callback != NULL);
    furi_check(expander_main != NULL);
    if(expander_main->handle) {
        furi_check(expander_main->type_c_up_sw_pg.callback == NULL);
        furi_bsp_set_callback(&expander_main->type_c_up_sw_pg, callback, context);
    } else {
        furi_bsp_show_error_message_main_expander();
    }
}

void furi_bsp_expander_main_attach_type_a_up_sw_pg_callback(FuriCallback callback, void* context) {
    furi_check(callback != NULL);
    furi_check(expander_main != NULL);
    if(expander_main->handle) {
        furi_check(expander_main->type_a_up_sw_pg.callback == NULL);
        furi_bsp_set_callback(&expander_main->type_a_up_sw_pg, callback, context);
    } else {
        furi_bsp_show_error_message_main_expander();
    }
}

void furi_bsp_expander_main_attach_expander7_callback(FuriCallback callback, void* context) {
    furi_check(callback != NULL);
    furi_check(expander_main != NULL);
    if(expander_main->handle) {
        furi_check(expander_main->expander7.callback == NULL);
        furi_bsp_set_callback(&expander_main->expander7, callback, context);
    } else {
        furi_bsp_show_error_message_main_expander();
    }
}
