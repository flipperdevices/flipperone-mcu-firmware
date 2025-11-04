#include "test.h"
#include <furi.h>
#include <input/input.h>

#define tag "TestSrv"

typedef struct {
    FuriPubSub* input;
    FuriPubSubSubscription* input_subscription;
    bool stop;
    uint32_t counter;
} InputTest;

static void input_test_events_callback(const void* value, void* ctx) {
    furi_assert(value);
    furi_assert(ctx);

    InputTest* instance = ctx;
    const InputEvent* event = value;

    FURI_LOG_I(tag, "Input event: key=%s type=%s", input_get_key_name(event->key), input_get_type_name(event->type));
}

int32_t test_srv(void* p) {
    UNUSED(p);

    InputTest* instance = malloc(sizeof(InputTest));
    instance->input = furi_record_open(RECORD_INPUT_EVENTS);
    instance->input_subscription =
        furi_pubsub_subscribe(instance->input, input_test_events_callback, instance);

    while(!instance->stop) {
        FURI_LOG_I(tag, "Test message");
        furi_delay_ms(1000);
    }

    furi_pubsub_unsubscribe(instance->input, instance->input_subscription);
    furi_record_close(RECORD_INPUT_EVENTS);
    free(instance);
    return 0;
}