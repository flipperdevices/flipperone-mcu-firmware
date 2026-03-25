#include "furi_hal_log.h"
#include "furi_hal_serial_control.h"
#include <SEGGER_RTT.h>

#define TAG "FuriHalLog"

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
    furi_log_add_handler(rtt_log_handler);

    const char* level_str = "unknown";
    furi_log_level_to_string(level, &level_str);
    FURI_LOG_I(TAG, "Logs initialized with level \"%s\" and output \"%s\"", level_str, log_output_to_string(output));
}

// call after hardware is ready to output logs
void furi_hal_log_hardware_init(void) {
    furi_hal_serial_control_set_logging_config(FuriHalSerialIdUart1, 230400);
    FURI_LOG_I(TAG, "Hardware log output initialized");
}
