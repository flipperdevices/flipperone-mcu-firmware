#include <furi.h>
#include "timer.h"
#include "common.h"
#include "pico/time.h"

timestamp_t get_time(void) {
    timestamp_t t;
    uint64_t us = to_us_since_boot(get_absolute_time());
    t.val = us;
    return t;
}

int crec_usleep(unsigned int us) {
    furi_delay_us(us);
    return EC_SUCCESS;
}
