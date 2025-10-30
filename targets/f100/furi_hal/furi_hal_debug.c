#include <furi_hal_debug.h>
#include <furi_hal_resources.h>

volatile bool furi_hal_debug_gdb_session_active = false;

void furi_hal_debug_enable(void) {
    // Low power mode debug
    //TODO: add implementation
}

void furi_hal_debug_disable(void) {
    // Low power mode debug
    //TODO: add implementation
}

bool furi_hal_debug_is_gdb_session_active(void) {
    return furi_hal_debug_gdb_session_active;
}
