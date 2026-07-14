#include "dmesg_app.h"
#include <furi.h>
#include <toolbox/circular_buffer.h>

#define TAG "DmesgApp"

#define DMESG_APP_LOG_BUFFER_SIZE (1024 * 10) // 10KB

struct DmesgApp {
    CircularBuffer* log_buffer;
    size_t read_index;
};

static DmesgApp* dmesg_app_head = NULL;

static void dmesg_app_log_tx_callback(const uint8_t* buffer, size_t size, void* context) {
    furi_assert(context);
    DmesgApp* instance = (DmesgApp*)context;
    furi_check(buffer);
    furi_check(size > 0);
    if(circular_buffer_acquire(instance->log_buffer)) {
        circular_buffer_write(instance->log_buffer, buffer, size);
        circular_buffer_release(instance->log_buffer);
    }
}

bool dmesg_app_get_log_data(DmesgApp* instance, uint8_t* data, size_t* size) {
    furi_check(instance);
    furi_check(data);
    furi_check(size);

    size_t received = circular_buffer_read(instance->log_buffer, data, *size);

    if(received > 0) {
        *size = received;
        return true;
    }

    *size = 0;
    return false;
}

void dmesg_app_update_read_index(DmesgApp* instance) {
    furi_check(instance);
    instance->read_index = circular_buffer_get_index_tail(instance->log_buffer);
}

void dmesg_app_restore_read_index(DmesgApp* instance) {
    furi_check(instance);
    circular_buffer_set_index_tail(instance->log_buffer, instance->read_index);
}

void dmesg_app_read_acquire(DmesgApp* instance) {
    furi_check(instance);
    circular_buffer_acquire(instance->log_buffer);
}

void dmesg_app_read_release(DmesgApp* instance) {
    furi_check(instance);
    circular_buffer_release(instance->log_buffer);
}

int32_t dmesg_app(void* p) {
    UNUSED(p);

    furi_check(dmesg_app_head == NULL);

    dmesg_app_head = malloc(sizeof(DmesgApp));
    dmesg_app_head->log_buffer = circular_buffer_alloc(DMESG_APP_LOG_BUFFER_SIZE, true);

    FuriLogHandler log_handler = {
        .callback = dmesg_app_log_tx_callback,
        .context = dmesg_app_head,
    };
    furi_log_add_handler(log_handler);

    furi_record_create(RECORD_DMESG_APP, dmesg_app_head);
    return 0;
}
