#include <furi.h>
#include <furi_hal_i2c_config.h>
#include <drivers/tps62868x/tps62868x.h>

#include <furi_bsp_vci.h>

#define TAG "BspVci"

/** Default voltage of the display VCI rail, in volts */
#define FURI_BSP_VCI_VOLTAGE_DEFAULT 3.3f

static Tps62868x* vci_power_supply = NULL;

static void furi_bsp_show_error_message_vci(void) {
    FURI_LOG_E(TAG, "Not initialized VCI power supply");
}

void furi_bsp_vci_init(void) {
    furi_check(vci_power_supply == NULL);

    vci_power_supply = tps62868x_init(&furi_hal_i2c_handle_control, TPS62868_ADDRESS);
    if(vci_power_supply) {
        FURI_LOG_I(TAG, "Initializing VCI power supply");
        tps62868x_set_voltage(vci_power_supply, FURI_BSP_VCI_VOLTAGE_DEFAULT);
    } else {
        FURI_LOG_E(TAG, "Failed to initialize VCI power supply");
    }
}

void furi_bsp_vci_deinit(void) {
    if(vci_power_supply) {
        tps62868x_deinit(vci_power_supply);
        vci_power_supply = NULL;
    } else {
        furi_bsp_show_error_message_vci();
    }
}

bool furi_bsp_vci_is_initialized(void) {
    return vci_power_supply != NULL;
}

void furi_bsp_vci_set_voltage(float voltage) {
    if(vci_power_supply) {
        tps62868x_set_voltage(vci_power_supply, voltage);
    } else {
        furi_bsp_show_error_message_vci();
    }
}

float furi_bsp_vci_get_voltage(void) {
    if(vci_power_supply) {
        return tps62868x_get_voltage(vci_power_supply);
    } else {
        furi_bsp_show_error_message_vci();
        return 0.0f;
    }
}
