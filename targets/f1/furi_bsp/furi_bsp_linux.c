#include "furi_bsp_linux.h"
#include "furi_bsp_expander.h"
#include <furi.h>

#define TAG "FuriHalBspLinux"

void furi_bsp_linux_reset(void) {
    furi_bsp_main_reset();
}

bool furi_bsp_linux_is_load(void) {
    const uint32_t mask = OutputExpMainUsb20Sel | OutputExpMainVcc5v0SysS5En;
    uint32_t status = furi_bsp_expander_main_read_output();
    return (status & mask) == mask;
}

void furi_bsp_linux_start(void) {
    uint32_t status = furi_bsp_expander_main_read_output();
    FURI_LOG_I(TAG, "Current expander output status: 0x%02lX", status);
    status |= OutputExpMainUsb20Sel | OutputExpMainVcc5v0SysS5En;
    FURI_LOG_I(TAG, "Setting expander output status: 0x%02lX", status);
    furi_bsp_expander_main_write_output(status);
    furi_bsp_expander_main_set_control(FuriBspControlExpanderMainCpu);
}

void furi_bsp_linux_maskrom(void) {
    uint32_t status = furi_bsp_expander_main_read_output();
    FURI_LOG_I(TAG, "Current expander output status: 0x%02lX", status);
    status |= OutputExpMainUsb20Sel | OutputExpMainVcc5v0SysS5En | OutputExpMainMaskromEn;
    FURI_LOG_I(TAG, "Setting expander output status: 0x%02lX", status);
    furi_bsp_expander_main_write_output(status);
    furi_bsp_expander_main_set_control(FuriBspControlExpanderMainCpu);
}
