#include <furi_hal_version.h>

bool furi_hal_version_do_i_belong_here(void) {
    // The target this firmware is built for (FW_TARGET f<N>) against the one
    // the device was programmed with. A device without OTP_DEVICE gives
    // nothing to compare with: take it as ours.
    return furi_hal_version_get_otp_version() == FuriHalVersionOtpVersionEmpty || furi_hal_version_get_hw_target() == version_get_target(version_get());
}
