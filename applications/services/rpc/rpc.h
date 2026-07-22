#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <furi.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RPC_BUFFER_SIZE (2048)

#define RECORD_RPC "rpc"

typedef struct Rpc Rpc;
typedef struct RpcSession RpcSession;

/** Callback to send bytes to client */
typedef void (*RpcSendBytesCallback)(void* context, uint8_t* bytes, size_t bytes_len);
/** Callback to notify transport layer that session was closed */
typedef void (*RpcSessionTerminatedCallback)(void* context);

/** RPC owner */
typedef enum {
    RpcOwnerCli = 0,
} RpcOwner;

/** Open RPC session
 *
 * @param   rpc     instance
 * @param   owner   owner of session
 * @return          pointer to RpcSession descriptor
 */
RpcSession* rpc_session_open(Rpc* rpc, RpcOwner owner);

/** Close RPC session
 *
 * @param   session     pointer to RpcSession descriptor
 */
void rpc_session_close(RpcSession* session);

/** Set session context for callbacks
 *
 * @param   session     pointer to RpcSession descriptor
 * @param   context     context to pass to callbacks
 */
void rpc_session_set_context(RpcSession* session, void* context);

/** Set callback to send bytes to client
 *
 * @param   session     pointer to RpcSession descriptor
 * @param   callback    callback (can be NULL)
 */
void rpc_session_set_send_bytes_callback(RpcSession* session, RpcSendBytesCallback callback);

/** Set callback to be called when RPC session is closed
 *
 * @param   session     pointer to RpcSession descriptor
 * @param   callback    callback (can be NULL)
 */
void rpc_session_set_terminated_callback(
    RpcSession* session,
    RpcSessionTerminatedCallback callback);

/** Give bytes to RPC service to decode and process
 *
 * @param   session     RPC session
 * @param   buffer      data bytes
 * @param   size        data size
 * @param   timeout     max wait time
 * @return              actually consumed bytes
 */
size_t rpc_session_feed(RpcSession* session, const uint8_t* buffer, size_t size, uint32_t timeout);

#ifdef __cplusplus
}
#endif
