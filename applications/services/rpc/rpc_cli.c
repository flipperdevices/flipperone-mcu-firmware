#include "rpc_i.h"
#include <containers/pipe.h>

#define TAG "RpcCli"

typedef struct {
    PipeSide* pipe;
    FuriSemaphore* terminate_semaphore;
} CliRpc;

#define CLI_READ_BUFFER_SIZE 64

static void rpc_cli_send_bytes_callback(void* context, uint8_t* bytes, size_t bytes_len) {
    furi_assert(context);
    furi_assert(bytes);
    furi_assert(bytes_len > 0);
    CliRpc* cli_rpc = (CliRpc*)context;
    pipe_send(cli_rpc->pipe, bytes, bytes_len);
}

static void rpc_cli_session_terminated_callback(void* context) {
    furi_check(context);
    CliRpc* cli_rpc = (CliRpc*)context;
    furi_semaphore_release(cli_rpc->terminate_semaphore);
}

void rpc_cli_command_start_session(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(args);
    furi_assert(pipe);
    furi_assert(context);
    Rpc* rpc = (Rpc*)context;

    RpcSession* rpc_session = rpc_session_open(rpc, RpcOwnerCli);
    if(rpc_session == NULL) {
        printf("Session start error\r\n");
        return;
    }

    /* Register message handlers */
    RpcHandler input_handler = {
        .message_handler = rpc_input_handler_callback,
        .context = NULL,
    };
    rpc_add_handler(rpc_session, Flipper_One_Rpc_RpcMessage_button_event_tag, &input_handler);

    RpcHandler touch_handler = {
        .message_handler = rpc_touch_handler_callback,
        .context = NULL,
    };
    rpc_add_handler(rpc_session, Flipper_One_Rpc_RpcMessage_touch_event_tag, &touch_handler);

    CliRpc cli_rpc = {.pipe = pipe, .terminate_semaphore = NULL};
    cli_rpc.terminate_semaphore = furi_semaphore_alloc(1, 0);
    rpc_session_set_context(rpc_session, &cli_rpc);
    rpc_session_set_send_bytes_callback(rpc_session, rpc_cli_send_bytes_callback);
    rpc_session_set_terminated_callback(rpc_session, rpc_cli_session_terminated_callback);

    /* Drain any leftover bytes in the pipe (e.g. trailing \n from the CLI
     * command that started this session).  Otherwise they get misinterpreted
     * as the varint length of the first protobuf message. */
    {
        uint8_t drain_buf[16];
        size_t avail = pipe_bytes_available(pipe);
        while(avail > 0) {
            size_t chunk = MIN(avail, sizeof(drain_buf));
            size_t n = pipe_receive(pipe, drain_buf, chunk);
            if(n == 0) break;
            FURI_LOG_D(TAG, "Drained %zu leftover byte(s)", n);
            avail = pipe_bytes_available(pipe);
        }
    }

    uint8_t* buffer = malloc(CLI_READ_BUFFER_SIZE);

    FURI_LOG_I(TAG, "Entering pipe read loop");
    while(1) {
        /* Read whatever is available right now.  First call blocks until
         * at least 1 byte arrives (or pipe breaks).  Subsequent calls
         * within this batch scoop up any bytes already buffered without
         * waiting, so a multi-byte message that arrives in separate USB
         * packets is still accumulated before feeding the session. */
        size_t size_received = pipe_receive(pipe, buffer, 1);
        if(pipe_state(pipe) != PipeStateOpen) break;

        while(size_received < CLI_READ_BUFFER_SIZE) {
            size_t avail = pipe_bytes_available(pipe);
            if(avail == 0) break;
            size_t chunk = MIN(avail, CLI_READ_BUFFER_SIZE - size_received);
            pipe_receive(pipe, buffer + size_received, chunk);
            size_received += chunk;
        }

        size_t fed_bytes = rpc_session_feed(rpc_session, buffer, size_received, 3000);
        furi_assert(fed_bytes == size_received);
    }

    FURI_LOG_I(TAG, "Pipe session ended");
    rpc_session_close(rpc_session);

    furi_check(furi_semaphore_acquire(cli_rpc.terminate_semaphore, FuriWaitForever) == FuriStatusOk);

    furi_semaphore_free(cli_rpc.terminate_semaphore);
    free(buffer);
}
