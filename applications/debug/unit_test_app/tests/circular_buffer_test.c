#include <furi.h>
#include <toolbox/circular_buffer.h>
#include "../unit_test_app.h"

#define TAG "CircularBufferTest"

//#define CB_DEBUG

#ifdef CB_DEBUG
#define CB_LOG(...) FURI_LOG_I(TAG, __VA_ARGS__)
#else
#define CB_LOG(...)
#endif

bool circular_buffer_test_write_read_overwrite_true(void) {
    uint32_t buffer_size = 8;
    CircularBuffer* cb = circular_buffer_alloc(buffer_size, true);
    TEST_ASSERT(cb);

    uint8_t data[16];
    size_t size;

    // Test writing and reading normal data
    const char* test_str = "Hello";

    for(size_t i = 0; i < buffer_size; ++i) {
        size = strlen(test_str);
        CB_LOG("Writing to circular buffer: size=%zu, data=%s", size, test_str);
        TEST_ASSERT(circular_buffer_write(cb, (const uint8_t*)test_str, size) == size);
        memset(data, 0, sizeof(data));
        size = sizeof(data);
        CB_LOG("Reading from circular buffer: size=%zu", circular_buffer_bytes_available(cb));
        TEST_ASSERT(circular_buffer_read(cb, data, size) == strlen(test_str));
        CB_LOG("Read from circular buffer: data=%s", data);
        TEST_ASSERT(strcmp((const char*)data, test_str) == 0);
    }

    // Test overwrite behavior with increasing input size
    char long_str[32];
    for(size_t i = 0; i < buffer_size; ++i) {
        size_t len_str = buffer_size + 1 + i; /* 9, 10, 11, ... */
        for(size_t j = 0; j < len_str; ++j)
            long_str[j] = '0' + (j % 10);
        long_str[len_str] = '\0';

        CB_LOG("Writing to circular buffer: size=%zu, data=%s", len_str, long_str);
        TEST_ASSERT(circular_buffer_write(cb, (const uint8_t*)long_str, len_str) == len_str);

        memset(data, 0, sizeof(data));
        size = sizeof(data);
        CB_LOG("Reading from circular buffer: size=%zu", circular_buffer_bytes_available(cb));
        TEST_ASSERT(circular_buffer_read(cb, data, size) == buffer_size);
        CB_LOG("Read from circular buffer: data=%s", data);

        TEST_ASSERT(strncmp((const char*)data, long_str + (len_str - (buffer_size)), buffer_size) == 0);
    }

    // Test write maximum size
    const char* max_str = "12345678";
    CB_LOG("Writing to circular buffer: size=%zu, data=%s", strlen(max_str), max_str);
    TEST_ASSERT(circular_buffer_write(cb, (const uint8_t*)max_str, strlen(max_str)) == strlen(max_str));
    memset(data, 0, sizeof(data));
    size = sizeof(data);
    CB_LOG("Reading from circular buffer: size=%zu", circular_buffer_bytes_available(cb));
    TEST_ASSERT(circular_buffer_read(cb, data, size) == strlen(max_str));
    CB_LOG("Read from circular buffer: data=%s", data);
    TEST_ASSERT(strcmp((const char*)data, max_str) == 0);

    // Test reading from an empty buffer
    size = sizeof(data);
    memset(data, 0, sizeof(data));
    CB_LOG("Reading from circular buffer: size=%zu", circular_buffer_bytes_available(cb));
    TEST_ASSERT(circular_buffer_read(cb, data, size) == 0);
    CB_LOG("Read from circular buffer: data=%s", data);

    // Test sequentially recording short segments without prior reading and reading the resulting data from the FIFO at the end
    const char* short_str = "abc#1234412";
    for(size_t i = 0; i < 5; ++i) {
        CB_LOG("Writing to circular buffer: size=%zu, data=%s", strlen(short_str), short_str);
        TEST_ASSERT(circular_buffer_write(cb, (const uint8_t*)short_str, strlen(short_str)) == strlen(short_str));
    }

    memset(data, 0, sizeof(data));
    size = sizeof(data);
    CB_LOG("Reading from circular buffer: size=%zu", circular_buffer_bytes_available(cb));
    TEST_ASSERT(circular_buffer_read(cb, data, size) == buffer_size);
    CB_LOG("Read from circular buffer: data=%s", data);

    /* Calculate expected: last `buffer_size` bytes of all concatenated writes */
    char full_str[128] = {0};
    for(size_t i = 0; i < 5; ++i)
        strcat(full_str, short_str);
    size_t full_len = strlen(full_str);
    const char* expected_data = full_str + full_len - buffer_size;

    TEST_ASSERT(strncmp((const char*)data, expected_data, buffer_size) == 0);

    circular_buffer_free(cb);
    return true;
}

bool circular_buffer_test_write_read_overwrite_false(void) {
    uint32_t buffer_size = 8;
    CircularBuffer* cb = circular_buffer_alloc(buffer_size, false);
    TEST_ASSERT(cb);

    uint8_t data[16];
    size_t size;

    // Test writing and reading normal data
    const char* test_str = "Hello";

    for(size_t i = 0; i < buffer_size; ++i) {
        size = strlen(test_str);
        CB_LOG("Writing to circular buffer: size=%zu, data=%s", size, test_str);
        TEST_ASSERT(circular_buffer_write(cb, (const uint8_t*)test_str, size) == size);
        memset(data, 0, sizeof(data));
        size = sizeof(data);
        CB_LOG("Reading from circular buffer: size=%zu", circular_buffer_bytes_available(cb));
        TEST_ASSERT(circular_buffer_read(cb, data, size) == strlen(test_str));
        CB_LOG("Read from circular buffer: data=%s", data);
        TEST_ASSERT(strcmp((const char*)data, test_str) == 0);
    }

    //test overwrite behavior with increasing input size
    char long_str[32];
    for(size_t i = 0; i < buffer_size; ++i) {
        size_t len_str = buffer_size + 1 + i; /* 9, 10, 11, ... */
        for(size_t j = 0; j < len_str; ++j)
            long_str[j] = '0' + (j % 10);
        long_str[len_str] = '\0';

        CB_LOG("Writing to circular buffer: size=%zu, data=%s", len_str, long_str);
        TEST_ASSERT(circular_buffer_write(cb, (const uint8_t*)long_str, len_str) == buffer_size);

        memset(data, 0, sizeof(data));
        size = sizeof(data);
        CB_LOG("Reading from circular buffer: size=%zu", circular_buffer_bytes_available(cb));
        TEST_ASSERT(circular_buffer_read(cb, data, size) == buffer_size);
        CB_LOG("Read from circular buffer: data=%s", data);

        /* overwrite=false: first `buffer_size` bytes of input are stored */
        TEST_ASSERT(strncmp((const char*)data, long_str, buffer_size) == 0);
    }

    // Test sequentially recording short segments without prior reading and reading the resulting data from the FIFO at the end
    const char* short_str = "a";
    for(size_t i = 0; i < 5; ++i) {
        CB_LOG("Writing to circular buffer: size=%zu, data=%s", strlen(short_str), short_str);
        TEST_ASSERT(circular_buffer_write(cb, (const uint8_t*)short_str, strlen(short_str)) == strlen(short_str));
    }
    memset(data, 0, sizeof(data));
    size = sizeof(data);
    size_t avail_before = circular_buffer_bytes_available(cb);
    CB_LOG("Reading from circular buffer: size=%zu", avail_before);
    TEST_ASSERT(circular_buffer_read(cb, data, size) == avail_before);
    CB_LOG("Read from circular buffer: data=%s", data);

    circular_buffer_free(cb);
    return true;
}

// --- entry point -----------------------------------------------------------

static const TestEntry cb_tests[] = {
    TEST_ENTRY(circular_buffer_test_write_read_overwrite_true),
    TEST_ENTRY(circular_buffer_test_write_read_overwrite_false),
};

static const size_t test_count = COUNT_OF(cb_tests);

bool circular_buffer_test_run(void) {
    FURI_LOG_I(TAG, "suite: start (%zu tests)", test_count);
    unsigned failed = 0;
    for(size_t i = 0; i < test_count; ++i) {
        if(!cb_tests[i].fn()) {
            FURI_LOG_E(TAG, "FAIL: %s", cb_tests[i].name);
            failed++;
        }
    }

    if(failed == 0) {
        FURI_LOG_I(TAG, "suite: PASS (%zu/%zu)", test_count, test_count);
        return true;
    } else {
        FURI_LOG_E(TAG, "suite: FAIL (%u/%zu failed)", failed, test_count);
        return false;
    }
}
