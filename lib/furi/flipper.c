#include "flipper.h"
#include <furi.h>
#include <version/version.h>
#include <furi_hal_version.h>
#include <furi_hal_nvm.h>

#include <FreeRTOS.h>

#include "applications.h"

#define TAG "Flipper"

static void flipper_print_version(const char* target, const Version* version) {
    if(version) {
        FURI_LOG_I(
            TAG,
            "\r\n\t%s version:\t%s\r\n"
            "\tBuild date:\t\t%s\r\n"
            "\tGit Commit:\t\t%s (%s)%s\r\n"
            "\tGit Branch:\t\t%s",
            target,
            version_get_version(version),
            version_get_builddate(version),
            version_get_githash(version),
            version_get_gitbranchnum(version),
            version_get_dirty_flag(version) ? " (dirty)" : "",
            version_get_gitbranch(version));
    } else {
        FURI_LOG_I(TAG, "No build info for %s", target);
    }
}

FURI_WEAK void flipper_init_services(void) {
    FURI_LOG_W(TAG, "flipper_init_services not implemented");
}

void flipper_init(void) {
    flipper_print_version("Firmware", furi_hal_version_get_firmware_version());

    FURI_LOG_I(TAG, "Boot mode %d", furi_hal_nvm_get_boot_mode());

    flipper_init_services();

     for(size_t i = 0; i < FLIPPER_SERVICES_COUNT; i++) {
        FURI_LOG_D(TAG, "Starting service %s", FLIPPER_SERVICES[i].name);

        FuriThread* thread = furi_thread_alloc_service(
            FLIPPER_SERVICES[i].name,
            FLIPPER_SERVICES[i].stack_size,
            FLIPPER_SERVICES[i].app,
            NULL);
        furi_thread_set_appid(thread, FLIPPER_SERVICES[i].appid);

        furi_thread_start(thread);
    }

    FURI_LOG_I(TAG, "Startup complete");
}
