#pragma once

#include <stdint.h>

#define task_wake(x)
#define task_wait_event(x)
#define task_set_event(task_id, event)

#define mutex_lock(x) //furi_mutex_lock(x)
#define mutex_unlock(x) //furi_mutex_unlock(x)

#define usleep(x) //furi_delay_us(x)

struct mutex {
	uint32_t lock;
	uint32_t waiters;
};

enum gpio_signal {
	GPIO_COUNT
};

/* Task event bitmasks */
/* Tasks may use the bits in TASK_EVENT_CUSTOM_BIT for their own events */
#define TASK_EVENT_CUSTOM_BIT(x) BUILD_CHECK_INLINE(BIT(x), BIT(x) & 0x0ffff)

/* Used to signal that sysjump preparation has completed */
#define TASK_EVENT_SYSJUMP_READY BIT(16)

/* Used to signal that IPC layer is available for sending new data */
#define TASK_EVENT_IPC_READY	BIT(17)

#define TASK_EVENT_PD_AWAKE	BIT(18)

/* npcx peci event */
#define TASK_EVENT_PECI_DONE	BIT(19)

/* I2C tx/rx interrupt handler completion event. */
#ifdef CHIP_STM32
#define TASK_EVENT_I2C_COMPLETION(port) \
				(1 << ((port) + 20))
#define TASK_EVENT_I2C_IDLE	(TASK_EVENT_I2C_COMPLETION(0))
#define TASK_EVENT_MAX_I2C	6
#ifdef I2C_PORT_COUNT
#if (I2C_PORT_COUNT > TASK_EVENT_MAX_I2C)
#error "Too many i2c ports for i2c events"
#endif
#endif
#else
#define TASK_EVENT_I2C_IDLE	BIT(20)
#define TASK_EVENT_PS2_DONE	BIT(21)
#endif

/* DMA transmit complete event */
#define TASK_EVENT_DMA_TC       BIT(26)
/* ADC interrupt handler event */
#define TASK_EVENT_ADC_DONE	BIT(27)
/*
 * task_reset() that was requested has been completed
 *
 * For test-only builds, may be used by some tasks to restart themselves.
 */
#define TASK_EVENT_RESET_DONE   BIT(28)
/* task_wake() called on task */
#define TASK_EVENT_WAKE		BIT(29)
/* Mutex unlocking */
#define TASK_EVENT_MUTEX	BIT(30)
/*
 * Timer expired.  For example, task_wait_event() timed out before receiving
 * another event.
 */
#define TASK_EVENT_TIMER	(1U << 31)

/* Maximum time for task_wait_event() */
#define TASK_MAX_WAIT_US 0x7fffffff