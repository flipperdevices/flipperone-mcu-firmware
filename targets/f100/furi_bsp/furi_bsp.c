#include "furi_bsp.h"
#include "furi_bsp_stdio.h"

void __assert_func(const char* file, int line, const char* func, const char* failedexpr) {
    FURI_LOG_E("Assert", " \"%s\" failed: file \"%s\", line %d%s%s\n", failedexpr, file, line, func ? ", function: " : "", func ? func : "");
    furi_crash("Assert failed");
}

void furi_bsp_init(void) {
    furi_bsp_expander_init();
    furi_bsp_stdio_init();
}
