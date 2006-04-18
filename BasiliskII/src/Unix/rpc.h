/*
 *  rpc.h - Remote Procedure Calls
 *
 *  Basilisk II (C) 1997-2006 Christian Bauer
 *  Contributed by Gwenole Beauchesne
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef RPC_H
#define RPC_H

// Error Types
enum {
  RPC_ERROR_NO_ERROR					= 0,
  RPC_ERROR_GENERIC						= -1000,
  RPC_ERROR_ERRNO_SET					= -1001,
  RPC_ERROR_NO_MEMORY					= -1002,
  RPC_ERROR_CONNECTION_NULL				= -1003,
  RPC_ERROR_CONNECTION_TYPE_MISMATCH	= -1004,
  RPC_ERROR_MESSAGE_TRUNCATED			= -1005,
  RPC_ERROR_MESSAGE_ARGUMENT_MISMATCH	= -1006,
  RPC_ERROR_MESSAGE_ARGUMENT_UNKNOWN	= -1007,
};

// Connection Handling
typedef struct rpc_connection_t rpc_connection_t;
extern rpc_connection_t *rpc_init_server(const char *ident);
extern rpc_connection_t *rpc_init_client(const char *ident);
extern int rpc_exit(rpc_connection_t *connection);
extern int rpc_listen_socket(rpc_connection_t *connection);
extern int rpc_listen(rpc_connection_t *connection);
extern int rpc_dispatch(rpc_connection_t *connection);
extern int rpc_wait_dispatch(rpc_connection_t *connection, int timeout);
extern int rpc_connection_busy(rpc_connection_t *connection);

// Message Passing
enum {
  RPC_TYPE_INVALID						=  0,
  RPC_TYPE_CHAR							= -2000,
  RPC_TYPE_BOOLEAN						= -2001,
  RPC_TYPE_INT32						= -2002,
  RPC_TYPE_UINT32						= -2003,
  RPC_TYPE_STRING						= -2004,
  RPC_TYPE_ARRAY						= -2005,
};
typedef struct rpc_message_t rpc_message_t;
extern int rpc_message_send_char(rpc_message_t *message, char c);
extern int rpc_message_send_int32(rpc_message_t *message, int32_t value);
extern int rpc_message_send_uint32(rpc_message_t *message, uint32_t value);
extern int rpc_message_send_string(rpc_message_t *message, const char *str);
extern int rpc_message_send_bytes(rpc_message_t *message, unsigned char *bytes, int count);
extern int rpc_message_recv_char(rpc_message_t *message, char *ret);
extern int rpc_message_recv_int32(rpc_message_t *message, int32_t *ret);
extern int rpc_message_recv_uint32(rpc_message_t *message, uint32_t *ret);
extern int rpc_message_recv_string(rpc_message_t *message, char **ret);
extern int rpc_message_recv_bytes(rpc_message_t *message, unsigned char *bytes, int count);
typedef int (*rpc_message_callback_t)(rpc_message_t *message, void *p_value);
typedef struct {
  int id;
  int size;
  rpc_message_callback_t send_callback;
  rpc_message_callback_t recv_callback;
} rpc_message_descriptor_t;
extern int rpc_message_add_callbacks(const rpc_message_descriptor_t *descs, int n_descs);

// Method Callbacks Handling
typedef int (*rpc_method_callback_t)(rpc_connection_t *connection);
typedef struct {
  int id;
  rpc_method_callback_t callback;
} rpc_method_descriptor_t;
extern int rpc_method_add_callbacks(rpc_connection_t *connection, const rpc_method_descriptor_t *descs, int n_descs);
extern int rpc_method_remove_callback_id(rpc_connection_t *connection, int id);
extern int rpc_method_remove_callbacks(rpc_connection_t *connection, const rpc_method_descriptor_t *descs, int n_descs);

// Remote Procedure Call (method invocation)
extern int rpc_method_invoke(rpc_connection_t *connection, int method, ...);
extern int rpc_method_wait_for_reply(rpc_connection_t *connection, ...);
extern int rpc_method_get_args(rpc_connection_t *connection, ...);
extern int rpc_method_send_reply(rpc_connection_t *connection, ...);

// Message Protocol
enum {
  RPC_METHOD_ERROR_ALERT = 1,
  RPC_METHOD_WARNING_ALERT,
  RPC_METHOD_EXIT
};

#endif /* RPC_H */
