/* Stubbed functions */
#include "stdint.h"

void cprintf(int zero, const char *fmt, ...)
{

}

void cprints(int zero, const char *fmt, ...)
{

}

uint32_t task_wait_event(int timeout_us)
{
    return 0;
}

uint32_t task_wait_event_mask(uint32_t event_mask, int timeout_us){
    return 0;
}

static inline void task_wake(uint32_t tskid)
{
	//task_set_event(tskid, 1);
}