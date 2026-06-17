#include "dmesg_app.h"
#include <furi.h>

#define TAG "DmesgApp"

#define DMESG_APP_LOG_BUFFER_SIZE      (1024 * 2)
#define DMESG_APP_LOG_BUFFER_POOL_SIZE (4)

struct DmesgApp {
    FuriStreamBuffer* log_buffer_pool[DMESG_APP_LOG_BUFFER_POOL_SIZE];
    uint8_t log_buffer_pool_index_write;
    uint8_t log_buffer_pool_index_read;
};

static DmesgApp* dmesg_app_head = NULL;

static void dmesg_app_log_tx_callback(const uint8_t* buffer, size_t size, void* context) {
    furi_assert(context);
    DmesgApp* instance = (DmesgApp*)context;
    furi_check(buffer);
    furi_check(size > 0);

    FuriStreamBuffer* log_buffer = instance->log_buffer_pool[instance->log_buffer_pool_index_write];
    size_t spaces_available = furi_stream_buffer_spaces_available(log_buffer);

    // Check if the log message can fit in the current buffer
    if(size < spaces_available) {
        furi_check(furi_stream_buffer_send(log_buffer, buffer, size, 0) == size);
    } else if(FURI_IS_IRQ_MODE()) {
        // From ISR: xStreamBufferReset is not ISR-safe, write what fits and discard the rest
        furi_stream_buffer_send(log_buffer, buffer, spaces_available, 0);
    } else {
        // Next buffer to write, we need to reset it before writing
        uint32_t next_index = (instance->log_buffer_pool_index_write + 1) % DMESG_APP_LOG_BUFFER_POOL_SIZE;
        FuriStreamBuffer* next_log_buffer = instance->log_buffer_pool[next_index];
        furi_stream_buffer_reset(next_log_buffer);

        // Write data
        furi_check(furi_stream_buffer_send(log_buffer, buffer, spaces_available, 0) == spaces_available);
        furi_check(furi_stream_buffer_send(next_log_buffer, buffer + spaces_available, size - spaces_available, 0) == (size - spaces_available));

        // Update write index to the next buffer
        instance->log_buffer_pool_index_write = next_index;
    }
}

bool dmesg_app_get_log_data(DmesgApp* instance, uint8_t* data, size_t* size) {
    FuriStreamBuffer* log_buffer = instance->log_buffer_pool[instance->log_buffer_pool_index_read];
    size_t bytes_available = furi_stream_buffer_bytes_available(log_buffer);

    if(bytes_available == 0) {
        if(instance->log_buffer_pool_index_read == instance->log_buffer_pool_index_write) {
            // No new data to read
            *size = 0;
            return false;
        }
        // update read index to the next buffer
        instance->log_buffer_pool_index_read = (instance->log_buffer_pool_index_read + 1) % DMESG_APP_LOG_BUFFER_POOL_SIZE;
        log_buffer = instance->log_buffer_pool[instance->log_buffer_pool_index_read];
        bytes_available = furi_stream_buffer_bytes_available(log_buffer);
    }

    if(bytes_available > 0) {
        if(bytes_available > *size) {
            // Data to read is larger than the provided buffer, we need to read in parts
            furi_check(furi_stream_buffer_receive(log_buffer, data, *size, 0) == *size);
            // The rest of the data will be read in the next call
        } else {
            furi_check(furi_stream_buffer_receive(log_buffer, data, bytes_available, 0) == bytes_available);
            *size = bytes_available;
        }
    } else {
        *size = 0;
    }

    return true;
}

void dmesg_app_update_read_index(DmesgApp* instance) {
    furi_check(instance);
    instance->log_buffer_pool_index_read = (instance->log_buffer_pool_index_write + 1) % DMESG_APP_LOG_BUFFER_POOL_SIZE;
}

int32_t dmesg_app(void* p) {
    UNUSED(p);

    furi_check(dmesg_app_head == NULL);

    dmesg_app_head = (DmesgApp*)malloc(sizeof(DmesgApp));
    for(uint32_t i = 0; i < DMESG_APP_LOG_BUFFER_POOL_SIZE; i++) {
        dmesg_app_head->log_buffer_pool[i] = furi_stream_buffer_alloc(DMESG_APP_LOG_BUFFER_SIZE, 1);
    }
    dmesg_app_head->log_buffer_pool_index_write = 0;
    dmesg_app_head->log_buffer_pool_index_read = 0;

    FuriLogHandler log_handler = {
        .callback = dmesg_app_log_tx_callback,
        .context = dmesg_app_head,
    };
    furi_log_add_handler(log_handler);

    furi_record_create(RECORD_DMESG_APP, dmesg_app_head);
    return 0;
}
