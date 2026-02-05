#include "furi_bsp.h"
#include "furi_bsp_stdio.h"

void furi_bsp_init(void) {
    furi_bsp_expander_init();
    furi_bsp_stdio_init();
}
