#include <furi_hal_version.h>

bool furi_hal_version_do_i_belong_here(void) {
    return furi_hal_version_get_hw_target() == 100;
}
