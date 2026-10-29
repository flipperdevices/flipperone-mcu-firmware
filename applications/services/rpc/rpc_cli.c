#include "rpc_i.h"
#include <containers/pipe.h>

#define TAG "RpcCli"

typedef struct {
    PipeSide* pipe;
    RpcSession* session;
    FuriEventLoop* event_loop;
    uint8_t* buffer;
    bool session_close_request;
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

static void rpc_cli_session_close_callback(void* context) {
    furi_assert(context);
    CliRpc* cli_rpc = (CliRpc*)context;
    cli_rpc->session_close_request = true;
    furi_event_loop_stop(cli_rpc->event_loop);
}

static void rpc_cli_data_arrived_callback(PipeSide* pipe, void* context) {
    furi_assert(context);
    CliRpc* cli_rpc = (CliRpc*)context;

    /* After a close request the bytes in the pipe are for the CLI shell,
     * so leave them there. */
    if(cli_rpc->session_close_request) {
        furi_event_loop_stop(cli_rpc->event_loop);
        return;
    }

    size_t size = MIN(pipe_bytes_available(pipe), CLI_READ_BUFFER_SIZE);
    size_t size_received = pipe_receive(pipe, cli_rpc->buffer, size);
    size_t fed_bytes = rpc_session_feed(cli_rpc->session, cli_rpc->buffer, size_received, 3000);
    furi_assert(fed_bytes == size_received);
}

static void rpc_cli_pipe_broken_callback(PipeSide* pipe, void* context) {
    UNUSED(pipe);
    furi_assert(context);
    CliRpc* cli_rpc = (CliRpc*)context;
    furi_event_loop_stop(cli_rpc->event_loop);
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

    /* Virtual display stream handlers */
    RpcHandler start_vd_handler = {
        .message_handler = rpc_start_virtual_display_handler,
        .context = rpc_session,
    };
    rpc_add_handler(rpc_session, Flipper_One_Rpc_RpcMessage_start_virtual_display_request_tag, &start_vd_handler);

    RpcHandler stop_vd_handler = {
        .message_handler = rpc_stop_virtual_display_handler,
        .context = rpc_session,
    };
    rpc_add_handler(rpc_session, Flipper_One_Rpc_RpcMessage_stop_virtual_display_request_tag, &stop_vd_handler);

    /* Session close handler */
    RpcHandler close_handler = {
        .message_handler = rpc_session_close_handler,
        .context = rpc_session,
    };
    rpc_add_handler(rpc_session, Flipper_One_Rpc_RpcMessage_rpc_session_close_request_tag, &close_handler);

    CliRpc cli_rpc = {.pipe = pipe, .session = rpc_session, .session_close_request = false};
    cli_rpc.terminate_semaphore = furi_semaphore_alloc(1, 0);
    cli_rpc.event_loop = furi_event_loop_alloc();
    cli_rpc.buffer = malloc(CLI_READ_BUFFER_SIZE);
    rpc_session_set_context(rpc_session, &cli_rpc);
    rpc_session_set_send_bytes_callback(rpc_session, rpc_cli_send_bytes_callback);
    rpc_session_set_close_callback(rpc_session, rpc_cli_session_close_callback);
    rpc_session_set_terminated_callback(rpc_session, rpc_cli_session_terminated_callback);

    FURI_LOG_I(TAG, "Entering pipe read loop");

    /* Send a ready marker so the host knows RPC mode is active */
    uint8_t ready = 0xFD;
    pipe_send(pipe, &ready, 1);

    /* Let pipe_send() notice a disconnect within 50 ms */
    pipe_set_state_check_period(pipe, 50);

    /* Only read what has arrived, so that the loop can stop on a close
     * request without taking the next byte away from the CLI shell. */
    pipe_attach_to_event_loop(pipe, cli_rpc.event_loop);
    pipe_set_callback_context(pipe, &cli_rpc);
    pipe_set_data_arrived_callback(pipe, rpc_cli_data_arrived_callback, 0);
    pipe_set_broken_callback(pipe, rpc_cli_pipe_broken_callback, 0);

    furi_event_loop_run(cli_rpc.event_loop);

    pipe_detach_from_event_loop(pipe);

    /* The session worker stops a screen stream before it ends */
    rpc_session_close(rpc_session);

    furi_check(
        furi_semaphore_acquire(cli_rpc.terminate_semaphore, FuriWaitForever) ==
        FuriStatusOk);

    FURI_LOG_I(TAG, "Pipe session ended");

    /* Freed after the session has terminated, so the close callback can't
     * run on a freed event loop. */
    furi_event_loop_free(cli_rpc.event_loop);
    furi_semaphore_free(cli_rpc.terminate_semaphore);
    free(cli_rpc.buffer);
}
