#pragma once
#include "rpc.h"
#include <pb.h>
#include <pb_decode.h>
#include <pb_encode.h>
#include <rpc.pb.h>
#include <containers/pipe.h>

/* Forward declarations */
typedef struct FuriString FuriString;
#ifdef __cplusplus
extern "C" {
#endif

typedef void (*RpcMessageHandler)(const Flipper_One_Rpc_RpcMessage* message, void* context);

typedef struct {
    RpcMessageHandler message_handler;
    void* context;
} RpcHandler;

void rpc_send(RpcSession* session, Flipper_One_Rpc_RpcMessage* message);

void rpc_send_and_release(RpcSession* session, Flipper_One_Rpc_RpcMessage* message);

void rpc_add_handler(RpcSession* session, pb_size_t message_tag, RpcHandler* handler);

/** Send pre-encoded bytes via the session output callback (no malloc inside). */
void rpc_send_preencoded(RpcSession* session, const uint8_t* bytes, size_t len);

/** Trigger the close callback if one is set (used by session close handler). */
void rpc_session_trigger_close_callback(RpcSession* session);

void rpc_cli_command_start_session(PipeSide* pipe, FuriString* args, void* context);

/* Message handlers */
void rpc_input_handler_callback(const Flipper_One_Rpc_RpcMessage* message, void* context);
void rpc_touch_handler_callback(const Flipper_One_Rpc_RpcMessage* message, void* context);
void rpc_start_virtual_display_handler(const Flipper_One_Rpc_RpcMessage* message, void* context);
void rpc_stop_virtual_display_handler(const Flipper_One_Rpc_RpcMessage* message, void* context);
void rpc_session_close_handler(const Flipper_One_Rpc_RpcMessage* message, void* context);

#ifdef SRV_RPC_DEBUG
void rpc_debug_print_data(const char* prefix, uint8_t* buffer, size_t size);
void rpc_debug_print_message(const Flipper_One_Rpc_RpcMessage* message);
#endif

#ifdef __cplusplus
}
#endif
