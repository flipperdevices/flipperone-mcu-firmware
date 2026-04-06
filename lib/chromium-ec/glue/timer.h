#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Time units in microseconds */
#define MSEC   1000
#define SECOND 1000000
#define SEC_UL 1000000ul
#define MINUTE 60000000
#define HOUR   3600000000ull /* Too big to fit in a signed int */

/* Microsecond timestamp. */
typedef union {
    uint64_t val;
    struct {
        uint32_t lo;
        uint32_t hi;
    } le /* little endian words */;
} timestamp_t;

/**
 * Get the current timestamp from the system timer.
 */
timestamp_t get_time(void);

/**
 * Sleep.
 *
 * The current task will be de-scheduled for at least the specified delay (and
 * perhaps longer, if a higher-priority task is running when the delay
 * expires).
 *
 * This may only be called from a task function, with interrupts enabled.
 *
 * @param us		Number of microseconds to sleep.
 * @return 0 on success, negative on error
 */
int crec_usleep(unsigned int us);

#ifdef __cplusplus
}
#endif
