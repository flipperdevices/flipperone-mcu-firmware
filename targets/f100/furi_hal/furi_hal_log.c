#include "furi_hal_log.h"
#include "furi_hal_serial_control.h"
#include <SEGGER_RTT.h>

#define TAG "FuriHalLog"

static struct {
    FuriLogLevel level;
    FuriHalLogOutput output;
} furi_hal_log_config;

static void furi_rtt_log_callback(const uint8_t* data, size_t size, void* context) {
    UNUSED(context);
    SEGGER_RTT_Write(0, data, size);
}

static FuriLogHandler rtt_log_handler = {
    .callback = furi_rtt_log_callback,
    .context = NULL,
};

static const char* log_output_to_string(FuriHalLogOutput output) {
    switch(output) {
    case FuriHalLogOutputNone:
        return "none";
    case FuriHalLogOutputRtt:
        return "RTT";
    case FuriHalLogOutputSerial:
        return "serial";
    case FuriHalLogOutputAll:
        return "all";
    default:
        return "unknown";
    }
}

// call as soon as possible in the boot process to capture early logs
void furi_hal_log_init(FuriLogLevel level, FuriHalLogOutput output) {
    furi_log_set_level(level);
    if(output == FuriHalLogOutputRtt || output == FuriHalLogOutputAll) {
        furi_log_add_handler(rtt_log_handler);
    }

    furi_hal_log_config.level = level;
    furi_hal_log_config.output = output;

    const char* level_str = "unknown";
    furi_log_level_to_string(furi_hal_log_config.level, &level_str);
    FURI_LOG_RAW_I("\r\n");
    FURI_LOG_I(TAG, "Logs initialized [level \"%s\", output \"%s\"]", level_str, log_output_to_string(furi_hal_log_config.output));
}

// call after hardware is ready to output logs
void furi_hal_log_hardware_init(void) {
    furi_hal_serial_control_init();
    if(furi_hal_log_config.output == FuriHalLogOutputSerial || furi_hal_log_config.output == FuriHalLogOutputAll) {
        furi_hal_serial_control_set_logging_config(FuriHalSerialIdUartPio, 1500000UL);
    }

    furi_log_puts("\n================================================================\n");

    const char* level_str = "unknown";
    furi_log_level_to_string(furi_hal_log_config.level, &level_str);
    FURI_LOG_RAW_I("\r\n");
    FURI_LOG_I(TAG, "Hardware log output initialized [level \"%s\", output \"%s\"]", level_str, log_output_to_string(furi_hal_log_config.output));
}
