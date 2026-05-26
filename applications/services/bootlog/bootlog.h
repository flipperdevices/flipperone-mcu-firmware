#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RECORD_BOOTLOG "bootlog"

/* Opaque forward declaration */
typedef struct FuriPubSub FuriPubSub;

/* Notification sent to FuriPubSub subscribers when new bootlog data arrives.*/
typedef struct {
    size_t new_head;
    size_t bytes_available;
} BootlogNotification;

/* Snapshot of ring buffer state */
typedef struct {
    uint8_t* buffer;
    size_t size;
    size_t head;
    size_t tail;
    size_t total;
    bool capturing;
} BootlogState;

void bootlog_start(void);
void bootlog_stop(void);
bool bootlog_get_state(BootlogState* state);
bool bootlog_is_capturing(void);

/**
 * Get the FuriPubSub instance for bootlog data-available notifications.
 * Subscribers receive BootlogNotification messages.
 */
FuriPubSub* bootlog_get_event_pubsub(void);
