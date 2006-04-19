/*
 *  rpc_unix.cpp - Remote Procedure Calls, Unix specific backend
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

/*
 *  NOTES:
 *  - this is subject to rewrite but the API is to be kept intact
 *  - this RPC system is very minimal and only suited for 1:1 communication
 *
 *  TODO:
 *  - better failure conditions
 *  - windows rpc
 */

#include "sysdeps.h"

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <netinet/in.h>

#include "rpc.h"

#define DEBUG 0
#include "debug.h"

#define NON_BLOCKING_IO 0

#if defined __linux__
#define USE_ABSTRACT_NAMESPACES 1
#endif


/* ====================================================================== */
/* === PThreads Glue                                                  === */
/* ====================================================================== */

//#define USE_THREADS

#ifndef USE_THREADS
#define pthread_t void *
#define pthread_cancel(th)
#define pthread_join(th, ret)
#define pthread_testcancel()
#define pthread_create(th, attr, start, arg) dummy_thread_create()
static inline int dummy_thread_create(void) { errno = ENOSYS; return -1; }

#undef  pthread_mutex_t
#define pthread_mutex_t volatile int
#undef  pthread_mutex_lock
#define pthread_mutex_lock(m) -1
#undef  pthread_mutex_unlock
#define pthread_mutex_unlock(m) -1
#undef  PTHREAD_MUTEX_INITIALIZER
#define PTHREAD_MUTEX_INITIALIZER 0
#endif


/* ====================================================================== */
/* === RPC Connection Handling                                        === */
/* ====================================================================== */

// Connection type
enum {
  RPC_CONNECTION_SERVER,
  RPC_CONNECTION_CLIENT,
};

// Connection status
enum {
  RPC_STATUS_IDLE,
  RPC_STATUS_BUSY,
};

// Client / Server connection
struct rpc_connection_t {
  int type;
  int status;
  int socket;
  char *socket_path;
  int server_socket;
  int server_thread_active;
  pthread_t server_thread;
  rpc_method_descriptor_t *callbacks;
  int n_callbacks;
  int send_offset;
  char send_buffer[BUFSIZ];
};

#define return_error(ERROR) do { error = (ERROR); goto do_return; } while (0)

// Set connection status (XXX protect connection with a lock?)
static inline void rpc_connection_set_status(rpc_connection_t *connection, int status)
{
  connection->status = status;
}

// Returns TRUE if the connection is busy (e.g. waiting for a reply)
int rpc_connection_busy(rpc_connection_t *connection)
{
  return connection && connection->status == RPC_STATUS_BUSY;
}

// Prepare socket path for addr.sun_path[]
static int _rpc_socket_path(char **pathp, const char *ident)
{
  int i, len;
  len = strlen(ident);

  if (pathp == NULL)
	return 0;

  char *path;
#if USE_ABSTRACT_NAMESPACES
  const int len_bias = 1;
  if ((path = (char *)malloc(len + len_bias + 1)) == NULL)
	return 0;
  path[0] = 0;
  strcpy(&path[len_bias], ident);
#else
  const int len_bias = 5;
  if ((path = (char *)malloc(len + len_bias + 1)) == NULL)
	return 0;
  strcpy(path, "/tmp/");
  for (i = 0; i < len; i++) {
    char ch = ident[i];
    if (ch == '/')
      ch = '_';
    path[len_bias + i] = ch;
  }
#endif
  len += len_bias;
  path[len] = '\0';
  if (*pathp)
	free(*pathp);
  *pathp = path;
  return len;
}

// Initialize server-side RPC system
rpc_connection_t *rpc_init_server(const char *ident)
{
  D(bug("rpc_init_server ident='%s'\n", ident));

  rpc_connection_t *connection;
  struct sockaddr_un addr;
  socklen_t addr_len;

  if (ident == NULL)
	return NULL;

  connection = (rpc_connection_t *)malloc(sizeof(*connection));
  if (connection == NULL)
	return NULL;
  connection->type = RPC_CONNECTION_SERVER;
  connection->status = RPC_STATUS_IDLE;
  connection->socket = -1;
  connection->server_thread_active = 0;
  connection->callbacks = NULL;
  connection->n_callbacks = 0;

  if ((connection->server_socket = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
	perror("server socket");
	free(connection);
	return NULL;
  }

  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  connection->socket_path = NULL;
  addr_len = _rpc_socket_path(&connection->socket_path, ident);
  memcpy(&addr.sun_path[0], connection->socket_path, addr_len);
  addr_len += sizeof(struct sockaddr_un) - sizeof(addr.sun_path);

  if (bind(connection->server_socket, (struct sockaddr *)&addr, addr_len) < 0) {
	perror("server bind");
	free(connection);
	return NULL;
  }

  if (listen(connection->server_socket, 1) < 0) {
	perror("server listen");
	free(connection);
	return NULL;
  }

  return connection;
}

// Initialize client-side RPC system
rpc_connection_t *rpc_init_client(const char *ident)
{
  D(bug("rpc_init_client ident='%s'\n", ident));

  rpc_connection_t *connection;
  struct sockaddr_un addr;
  socklen_t addr_len;

  if (ident == NULL)
	return NULL;

  connection = (rpc_connection_t *)malloc(sizeof(*connection));
  if (connection == NULL)
	return NULL;
  connection->type = RPC_CONNECTION_CLIENT;
  connection->status = RPC_STATUS_IDLE;
  connection->server_socket = -1;
  connection->callbacks = NULL;
  connection->n_callbacks = 0;

  if ((connection->socket = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
	perror("client socket");
	free(connection);
	return NULL;
  }

  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  connection->socket_path = NULL;
  addr_len = _rpc_socket_path(&connection->socket_path, ident);
  memcpy(&addr.sun_path[0], connection->socket_path, addr_len);
  addr_len += sizeof(struct sockaddr_un) - sizeof(addr.sun_path);

  // Wait at most 5 seconds for server to initialize
  const int N_CONNECT_WAIT_DELAY = 10;
  int n_connect_attempts = 5000 / N_CONNECT_WAIT_DELAY;
  if (n_connect_attempts == 0)
	n_connect_attempts = 1;
  while (n_connect_attempts > 0) {
	if (connect(connection->socket, (struct sockaddr *)&addr, addr_len) == 0)
	  break;
	if (n_connect_attempts > 1 && errno != ECONNREFUSED && errno != ENOENT) {
	  perror("client_connect");
	  free(connection);
	  return NULL;
	}
	n_connect_attempts--;
	usleep(N_CONNECT_WAIT_DELAY);
  }
  if (n_connect_attempts == 0) {
	free(connection);
	return NULL;
  }

  return connection;
}

// Close RPC connection
int rpc_exit(rpc_connection_t *connection)
{
  D(bug("rpc_exit\n"));

  if (connection == NULL)
	return RPC_ERROR_CONNECTION_NULL;

  if (connection->socket_path) {
	if (connection->socket_path[0])
	  unlink(connection->socket_path);
	free(connection->socket_path);
  }

  if (connection->type == RPC_CONNECTION_SERVER) {
	if (connection->server_thread_active) {
	  pthread_cancel(connection->server_thread);
	  pthread_join(connection->server_thread, NULL);
	}
	if (connection->socket != -1)
	  close(connection->socket);
	if (connection->server_socket != -1)
	  close(connection->server_socket);
  }
  else {
	if (connection->socket != -1)
	  close(connection->socket);
  }

  if (connection->callbacks)
	free(connection->callbacks);
  free(connection);

  return RPC_ERROR_NO_ERROR;
}

// Wait for a message to arrive on the connection port
static inline int _rpc_wait_dispatch(rpc_connection_t *connection, int timeout)
{
	struct timeval tv;
	tv.tv_sec  = timeout / 1000000;
	tv.tv_usec = timeout % 1000000;

	fd_set rfds;
	FD_ZERO(&rfds);
	FD_SET(connection->socket, &rfds);
	return select(connection->socket + 1, &rfds, NULL, NULL, &tv);
}

int rpc_wait_dispatch(rpc_connection_t *connection, int timeout)
{
	if (connection == NULL)
		return RPC_ERROR_CONNECTION_NULL;
	if (connection->type != RPC_CONNECTION_SERVER)
		return RPC_ERROR_CONNECTION_TYPE_MISMATCH;

	return _rpc_wait_dispatch(connection, timeout);
}

// Process incoming messages in the background
static void *rpc_server_func(void *arg)
{
  rpc_connection_t *connection = (rpc_connection_t *)arg;

  int ret = rpc_listen_socket(connection);
  if (ret < 0)
	return NULL;

  connection->server_thread_active = 1;
  for (;;) {
	// XXX broken MacOS X doesn't implement cancellation points correctly
	pthread_testcancel();

	// wait for data to arrive
	int ret = _rpc_wait_dispatch(connection, 50000);
	if (ret == 0)
	  continue;
	if (ret < 0)
	  break;

	rpc_dispatch(connection);
  }
  connection->server_thread_active = 0;
  return NULL;
}

// Return listen socket of RPC connection
int rpc_listen_socket(rpc_connection_t *connection)
{
  D(bug("rpc_listen_socket\n"));

  if (connection == NULL)
	return RPC_ERROR_CONNECTION_NULL;
  if (connection->type != RPC_CONNECTION_SERVER)
	return RPC_ERROR_CONNECTION_TYPE_MISMATCH;

  struct sockaddr_un addr;
  socklen_t addr_len = sizeof(addr);
  if ((connection->socket = accept(connection->server_socket, (struct sockaddr *)&addr, &addr_len)) < 0) {
	perror("server accept");
	return RPC_ERROR_ERRNO_SET;
  }

#if NON_BLOCKING_IO
  int val = fcntl(connection->socket, F_GETFL, 0);
  if (val < 0) {
	perror("server fcntl F_GETFL");
	return RPC_ERROR_ERRNO_SET;
  }
  if (fcntl(connection->socket, F_SETFL, val | O_NONBLOCK) < 0) {
	perror("server fcntl F_SETFL");
	return RPC_ERROR_ERRNO_SET;
  }
#endif

  return connection->socket;
}

// Listen for incoming messages on RPC connection
#ifdef USE_THREADS
int rpc_listen(rpc_connection_t *connection)
{
  D(bug("rpc_listen\n"));

  if (pthread_create(&connection->server_thread, NULL, rpc_server_func, connection) != 0) {
	perror("server thread");
	return RPC_ERROR_ERRNO_SET;
  }

  return RPC_ERROR_NO_ERROR;
}
#endif


/* ====================================================================== */
/* === Message Passing                                                === */
/* ====================================================================== */

// Message markers
enum {
  RPC_MESSAGE_START		= -3000,
  RPC_MESSAGE_END		= -3001,
  RPC_MESSAGE_ACK		= -3002,
  RPC_MESSAGE_REPLY		= -3003,
  RPC_MESSAGE_FAILURE	= -3004,
};

// Message type
struct rpc_message_t {
  int socket;
  int offset;
  unsigned char buffer[BUFSIZ];
};

// User-defined marshalers
static struct {
  rpc_message_descriptor_t *descs;
  int last;
  int count;
} g_message_descriptors = { NULL, 0, 0 };
static pthread_mutex_t g_message_descriptors_lock = PTHREAD_MUTEX_INITIALIZER;

// Add a user-defined marshaler
static int rpc_message_add_callback(const rpc_message_descriptor_t *desc)
{
  D(bug("rpc_message_add_callback\n"));

  const int N_ENTRIES_ALLOC = 8;
  int error = RPC_ERROR_NO_ERROR;

  pthread_mutex_lock(&g_message_descriptors_lock);
  if (g_message_descriptors.descs == NULL) {
	g_message_descriptors.count = N_ENTRIES_ALLOC;
	if ((g_message_descriptors.descs = (rpc_message_descriptor_t *)malloc(g_message_descriptors.count * sizeof(g_message_descriptors.descs[0]))) == NULL) {
	  pthread_mutex_unlock(&g_message_descriptors_lock);
	  return RPC_ERROR_NO_MEMORY;
	}
	g_message_descriptors.last = 0;
  }
  else if (g_message_descriptors.last >= g_message_descriptors.count) {
	g_message_descriptors.count += N_ENTRIES_ALLOC;
	if ((g_message_descriptors.descs = (rpc_message_descriptor_t *)realloc(g_message_descriptors.descs, g_message_descriptors.count * sizeof(g_message_descriptors.descs[0]))) == NULL) {
	  pthread_mutex_unlock(&g_message_descriptors_lock);
	  return RPC_ERROR_NO_MEMORY;
	}
  }

  // XXX only one callback per ID
  int i;
  for (i = 0; i < g_message_descriptors.last; i++) {
	if (g_message_descriptors.descs[i].id == desc->id) {
	  pthread_mutex_unlock(&g_message_descriptors_lock);
	  return RPC_ERROR_NO_ERROR;
	}
  }

  g_message_descriptors.descs[g_message_descriptors.last++] = *desc;
  pthread_mutex_unlock(&g_message_descriptors_lock);
  return error;
}

// Add user-defined marshalers
int rpc_message_add_callbacks(const rpc_message_descriptor_t *descs, int n_descs)
{
  D(bug("rpc_message_add_callbacks\n"));

  int i, error;
  for (i = 0; i < n_descs; i++) {
	if ((error = rpc_message_add_callback(&descs[i])) < 0)
	  return error;
  }

  return RPC_ERROR_NO_ERROR;
}

// Find user-defined marshaler
static rpc_message_descriptor_t *rpc_message_find_descriptor(int id)
{
  D(bug("rpc_message_find_descriptor\n"));

  if (g_message_descriptors.descs) {
	int i;
	for (i = 0; i < g_message_descriptors.count; i++) {
	  if (g_message_descriptors.descs[i].id == id)
		return &g_message_descriptors.descs[i];
	}
  }

  return NULL;
}

// Initialize message
static inline void rpc_message_init(rpc_message_t *message, rpc_connection_t *connection)
{
  message->socket = connection->socket;
  message->offset = 0;
}

// Send BYTES
static inline int _rpc_message_send_bytes(rpc_message_t *message, unsigned char *bytes, int count)
{
  if (send(message->socket, bytes, count, 0) != count)
	return RPC_ERROR_ERRNO_SET;
  return RPC_ERROR_NO_ERROR;
}

// Send message on wire
static inline int rpc_message_flush(rpc_message_t *message)
{
  int error = _rpc_message_send_bytes(message, message->buffer, message->offset);
  message->offset = 0;
  return error;
}

// Send BYTES (public interface, may need to flush internal buffer)
int rpc_message_send_bytes(rpc_message_t *message, unsigned char *bytes, int count)
{
  if (message->offset > 0) {
	int error = rpc_message_flush(message);
	if (error != RPC_ERROR_NO_ERROR)
	  return error;
  }
  return _rpc_message_send_bytes(message, bytes, count);
}

// Send BYTES (buffered)
static inline void _rpc_message_send_bytes_buffered(rpc_message_t *message, unsigned char *bytes, int count)
{
  memcpy(&message->buffer[message->offset], bytes, count);
  message->offset += count;
}

// Send CHAR
int rpc_message_send_char(rpc_message_t *message, char c)
{
  D(bug("  send CHAR '%c'\n", c));

  unsigned char e_value = c;
  if (message->offset + sizeof(e_value) >= sizeof(message->buffer)) {
	int error = rpc_message_flush(message);
	if (error != RPC_ERROR_NO_ERROR)
	  return error;
  }
  _rpc_message_send_bytes_buffered(message, (unsigned char *)&e_value, sizeof(e_value));
  return RPC_ERROR_NO_ERROR;
}

// Send INT32
int rpc_message_send_int32(rpc_message_t *message, int32_t value)
{
  D(bug("  send INT32 %d\n", value));

  int32_t e_value = htonl(value);
  if (message->offset + sizeof(e_value) >= sizeof(message->buffer)) {
	int error = rpc_message_flush(message);
	if (error != RPC_ERROR_NO_ERROR)
	  return error;
  }
  _rpc_message_send_bytes_buffered(message, (unsigned char *)&e_value, sizeof(e_value));
  return RPC_ERROR_NO_ERROR;
}

// Send UINT32
int rpc_message_send_uint32(rpc_message_t *message, uint32_t value)
{
  D(bug("  send UINT32 %u\n", value));

  uint32_t e_value = htonl(value);
  if (message->offset + sizeof(e_value) >= sizeof(message->buffer)) {
	int error = rpc_message_flush(message);
	if (error != RPC_ERROR_NO_ERROR)
	  return error;
  }
  _rpc_message_send_bytes_buffered(message, (unsigned char *)&e_value, sizeof(e_value));
  return RPC_ERROR_NO_ERROR;
}

// Send STRING
int rpc_message_send_string(rpc_message_t *message, const char *str)
{
  D(bug("  send STRING \"%s\"\n", str));

  int error, length = str ? strlen(str) : 0;
  uint32_t e_value = htonl(length);
  if (message->offset + sizeof(e_value) >= sizeof(message->buffer)) {
	error = rpc_message_flush(message);
	if (error != RPC_ERROR_NO_ERROR)
	  return error;
  }
  _rpc_message_send_bytes_buffered(message, (unsigned char *)&e_value, sizeof(e_value));
  error = rpc_message_flush(message);
  if (error != RPC_ERROR_NO_ERROR)
	return error;
  D(bug("str=%p\n", str));
  return _rpc_message_send_bytes(message, (unsigned char *)str, length);
}

// Send message arguments
static int rpc_message_send_args(rpc_message_t *message, va_list args)
{
  int type;
  rpc_message_descriptor_t *desc;
  while ((type = va_arg(args, int)) != RPC_TYPE_INVALID) {
	int error = rpc_message_send_int32(message, type);
	if (error != RPC_ERROR_NO_ERROR)
	  return error;
	switch (type) {
	case RPC_TYPE_CHAR:
	  error = rpc_message_send_char(message, (char )va_arg(args, int));
	  break;
	case RPC_TYPE_BOOLEAN:
	case RPC_TYPE_INT32:
	  error = rpc_message_send_int32(message, va_arg(args, int));
	  break;
	case RPC_TYPE_UINT32:
	  error = rpc_message_send_uint32(message, va_arg(args, unsigned int));
	  break;
	case RPC_TYPE_STRING:
	  error = rpc_message_send_string(message, va_arg(args, char *));
	  break;
	case RPC_TYPE_ARRAY: {
	  int i;
	  int array_type = va_arg(args, int32_t);
	  int array_size = va_arg(args, uint32_t);
	  if ((error = rpc_message_send_int32(message, array_type)) < 0)
		return error;
	  if ((error = rpc_message_send_uint32(message, array_size)) < 0)
		return error;
	  switch (array_type) {
	  case RPC_TYPE_CHAR: {
		unsigned char *array = va_arg(args, unsigned char *);
		error = rpc_message_flush(message);
		if (error != RPC_ERROR_NO_ERROR)
		  return error;
		error = _rpc_message_send_bytes(message, array, array_size);
		break;
	  }
	  case RPC_TYPE_BOOLEAN:
	  case RPC_TYPE_INT32: {
		int32_t *array = va_arg(args, int32_t *);
		for (i = 0; i < array_size; i++) {
		  if ((error = rpc_message_send_int32(message, array[i])) < 0)
			break;
		}
		break;
	  }
	  case RPC_TYPE_UINT32: {
		uint32_t *array = va_arg(args, uint32_t *);
		for (i = 0; i < array_size; i++) {
		  if ((error = rpc_message_send_uint32(message, array[i])) < 0)
			break;
		}
		break;
	  }
	  case RPC_TYPE_STRING: {
		char **array = va_arg(args, char **);
		for (i = 0; i < array_size; i++) {
		  if ((error = rpc_message_send_string(message, array[i])) < 0)
			break;
		}
		break;
	  }
	  default:
		if ((desc = rpc_message_find_descriptor(array_type)) != NULL) {
		  uint8_t *array = va_arg(args, uint8_t *);
		  for (i = 0; i < array_size; i++) {
			if ((error = desc->send_callback(message, &array[i * desc->size])) < 0)
			  break;
		  }
		}
		else {
		  fprintf(stderr, "unknown array arg type %d to send\n", type);
		  error = RPC_ERROR_MESSAGE_ARGUMENT_UNKNOWN;
		}
		break;
	  }
	  break;
	}
	default:
	  if ((desc = rpc_message_find_descriptor(type)) != NULL)
		error = desc->send_callback(message, va_arg(args, uint8_t *));
	  else {
		fprintf(stderr, "unknown arg type %d to send\n", type);
		error = RPC_ERROR_MESSAGE_ARGUMENT_UNKNOWN;
	  }
	  break;
	}
	if (error != RPC_ERROR_NO_ERROR)
	  return error;
  }
  return RPC_ERROR_NO_ERROR;
}

// Receive raw BYTES
static inline int _rpc_message_recv_bytes(rpc_message_t *message, unsigned char *bytes, int count)
{
  do {
	int n = recv(message->socket, bytes, count, 0);
	if (n > 0) {
	  count -= n;
	  bytes += n;
	}
	else if (n == -1 && errno == EINTR)
	  continue;
	else {
#if NON_BLOCKING_IO
	  if (errno == EAGAIN || errno == EWOULDBLOCK) {
		// wait for data to arrive
		fd_set rfds;
		FD_ZERO(&rfds);
		FD_SET(message->socket, &rfds);
		int ret = select(message->socket + 1, &rfds, NULL, NULL, NULL);
		if (ret > 0)
		  continue;
	  }
#endif
	  return RPC_ERROR_ERRNO_SET;
	}
  } while (count > 0);
  return RPC_ERROR_NO_ERROR;
}

int rpc_message_recv_bytes(rpc_message_t *message, unsigned char *bytes, int count)
{
  return _rpc_message_recv_bytes(message, bytes, count);
}

// Receive CHAR
int rpc_message_recv_char(rpc_message_t *message, char *ret)
{
  char r_value;
  int error;
  if ((error = _rpc_message_recv_bytes(message, (unsigned char *)&r_value, sizeof(r_value))) < 0)
	return error;
  *ret = r_value;
  D(bug("  recv CHAR '%c'\n", *ret));
  return RPC_ERROR_NO_ERROR;
}

// Receive INT32
int rpc_message_recv_int32(rpc_message_t *message, int32_t *ret)
{
  int32_t r_value;
  int error;
  if ((error = _rpc_message_recv_bytes(message, (unsigned char *)&r_value, sizeof(r_value))) < 0)
	return error;
  *ret = ntohl(r_value);
  D(bug("  recv INT32 %d\n", *ret));
  return RPC_ERROR_NO_ERROR;
}

// Receive UINT32
int rpc_message_recv_uint32(rpc_message_t *message, uint32_t *ret)
{
  uint32_t r_value;
  int error;
  if ((error = _rpc_message_recv_bytes(message, (unsigned char *)&r_value, sizeof(r_value))) < 0)
	return error;
  *ret = ntohl(r_value);
  D(bug("  recv UINT32 %u\n", *ret));
  return RPC_ERROR_NO_ERROR;
}

// Receive STRING
int rpc_message_recv_string(rpc_message_t *message, char **ret)
{
  char *str;
  int length;
  uint32_t r_value;
  int error;
  if ((error = _rpc_message_recv_bytes(message, (unsigned char *)&r_value, sizeof(r_value))) < 0)
	return error;
  length = ntohl(r_value);
  if (length == 0)
	str = NULL;
  else {
	if ((str = (char *)malloc(length + 1)) == NULL)
	  return RPC_ERROR_NO_MEMORY;
	if ((error = _rpc_message_recv_bytes(message, (unsigned char *)str, length)) < 0)
	  return error;
	str[length] = '\0';
  }
  *ret = str;
  D(bug("  recv STRING \"%s\"\n", *ret));
  return RPC_ERROR_NO_ERROR;
}

// Receive message arguments
static int rpc_message_recv_args(rpc_message_t *message, va_list args)
{
  int expected_type, error;
  rpc_message_descriptor_t *desc;

  while ((expected_type = va_arg(args, int)) != RPC_TYPE_INVALID) {
	void *p_value = va_arg(args, void *);
	int32_t type;
	if ((error = rpc_message_recv_int32(message, &type)) < 0)
	  return error;
	if (type != expected_type)
	  return RPC_ERROR_MESSAGE_ARGUMENT_MISMATCH;
	switch (type) {
	case RPC_TYPE_CHAR:
	  error = rpc_message_recv_char(message, (char *)p_value);
	  break;
	case RPC_TYPE_BOOLEAN:
	case RPC_TYPE_INT32:
	  error = rpc_message_recv_int32(message, (int32_t *)p_value);
	  break;
	case RPC_TYPE_UINT32:
	  error = rpc_message_recv_uint32(message, (uint32_t *)p_value);
	  break;
	case RPC_TYPE_STRING:
	  error = rpc_message_recv_string(message, (char **)p_value);
	  break;
	case RPC_TYPE_ARRAY: {
	  int i;
	  int32_t array_type;
	  uint32_t array_size;
	  if ((error = rpc_message_recv_int32(message, &array_type)) < 0)
		return error;
	  if ((error = rpc_message_recv_uint32(message, &array_size)) < 0)
		return error;
	  p_value = va_arg(args, void *);
	  *((uint32_t *)p_value) = array_size;
	  p_value = va_arg(args, void *);
	  switch (array_type) {
	  case RPC_TYPE_CHAR: {
		unsigned char *array;
		if ((array = (unsigned char *)malloc(array_size * sizeof(*array))) == NULL)
		  return RPC_ERROR_NO_MEMORY;
		error = _rpc_message_recv_bytes(message, array, array_size);
		if (error != RPC_ERROR_NO_ERROR)
		  return error;
		*((void **)p_value) = (void *)array;
		break;
	  }
	  case RPC_TYPE_BOOLEAN:
	  case RPC_TYPE_INT32: {
		int *array;
		if ((array = (int *)malloc(array_size * sizeof(*array))) == NULL)
		  return RPC_ERROR_NO_MEMORY;
		for (i = 0; i < array_size; i++) {
		  int32_t value;
		  if ((error = rpc_message_recv_int32(message, &value)) < 0)
			return error;
		  array[i] = value;
		}
		*((void **)p_value) = (void *)array;
		break;
	  }
	  case RPC_TYPE_UINT32: {
		unsigned int *array;
		if ((array = (unsigned int *)malloc(array_size * sizeof(*array))) == NULL)
		  return RPC_ERROR_NO_MEMORY;
		for (i = 0; i < array_size; i++) {
		  uint32_t value;
		  if ((error = rpc_message_recv_uint32(message, &value)) < 0)
			return error;
		  array[i] = value;
		}
		*((void **)p_value) = (void *)array;
		break;
	  }
	  case RPC_TYPE_STRING: {
		char **array;
		if ((array = (char **)malloc(array_size * sizeof(*array))) == NULL)
		  return RPC_ERROR_NO_MEMORY;
		for (i = 0; i < array_size; i++) {
		  char *str;
		  if ((error = rpc_message_recv_string(message, &str)) < 0)
			return error;
		  array[i] = str;
		}
		*((void **)p_value) = (void *)array;
		break;
	  }
	  default:
		if ((desc = rpc_message_find_descriptor(array_type)) != NULL) {
		  char *array;
		  if ((array = (char *)malloc(array_size * desc->size)) == NULL)
			return RPC_ERROR_NO_MEMORY;
		  for (i = 0; i < array_size; i++) {
			if ((error = desc->recv_callback(message, &array[i * desc->size])) < 0)
			  return error;
		  }
		  *((void **)p_value) = array;
		}
		else {
		  fprintf(stderr, "unknown array arg type %d to receive\n", type);
		  error = RPC_ERROR_MESSAGE_ARGUMENT_UNKNOWN;
		}
		break;
	  }
	  break;
	}
	default:
	  if ((desc = rpc_message_find_descriptor(type)) != NULL)
		error = desc->recv_callback(message, p_value);
	  else {
		fprintf(stderr, "unknown arg type %d to send\n", type);
		error = RPC_ERROR_MESSAGE_ARGUMENT_UNKNOWN;
	  }
	  break;
	}
	if (error != RPC_ERROR_NO_ERROR)
	  return error;
  }
  return RPC_ERROR_NO_ERROR;
}

// Skip message argument
static int rpc_message_skip_arg(rpc_message_t *message, int type)
{
  unsigned char dummy[BUFSIZ];
  int error = RPC_ERROR_GENERIC;
  switch (type) {
  case RPC_TYPE_CHAR:
	error = _rpc_message_recv_bytes(message, dummy, 1);
	break;
  case RPC_TYPE_BOOLEAN:
  case RPC_TYPE_INT32:
  case RPC_TYPE_UINT32:
	error = _rpc_message_recv_bytes(message, dummy, 4);
	break;
  case RPC_TYPE_STRING: {
	int32_t length;
	if ((error = rpc_message_recv_int32(message, &length)) < 0)
	  return error;
	while (length >= sizeof(dummy)) {
	  if ((error = _rpc_message_recv_bytes(message, dummy, sizeof(dummy))) < 0)
		return error;
	  length -= sizeof(dummy);
	}
	if (length > 0) {
	  if ((error = _rpc_message_recv_bytes(message, dummy, length)) < 0)
		return error;
	}
	break;
  }
  default:
	fprintf(stderr, "unknown arg type %d to receive\n", type);
	break;
  }
  return error;
}

// Dispatch message received in the server loop
int rpc_dispatch(rpc_connection_t *connection)
{
  rpc_message_t message;
  rpc_message_init(&message, connection);

  int32_t method, value, ret = RPC_MESSAGE_FAILURE;
  if (rpc_message_recv_int32(&message, &value) != RPC_ERROR_NO_ERROR &&
	  value != RPC_MESSAGE_START)
	return ret;

  D(bug("receiving message\n"));
  if (rpc_message_recv_int32(&message, &method) == RPC_ERROR_NO_ERROR &&
	  connection->callbacks != NULL) {
	int i;
	for (i = 0; i < connection->n_callbacks; i++) {
	  if (connection->callbacks[i].id == method) {
		if (connection->callbacks[i].callback &&
			connection->callbacks[i].callback(connection) == RPC_ERROR_NO_ERROR) {
		  if (rpc_message_recv_int32(&message, &value) == RPC_ERROR_NO_ERROR && value == RPC_MESSAGE_END)
			ret = RPC_MESSAGE_ACK;
		  else {
			fprintf(stderr, "corrupted message handler %d\n", method);
			for (;;) {
			  if (rpc_message_skip_arg(&message, value) != RPC_ERROR_NO_ERROR)
				break;
			  if (rpc_message_recv_int32(&message, &value) != RPC_ERROR_NO_ERROR)
				break;
			  if (value == RPC_MESSAGE_END)
				break;
			}
		  }
		  break;
		}
	  }
	}
  }
  rpc_message_send_int32(&message, ret);
  rpc_message_flush(&message);
  D(bug("  -- message received\n"));
  return ret == RPC_MESSAGE_ACK ? method : ret;
}


/* ====================================================================== */
/* === Method Callbacks Handling                                      === */
/* ====================================================================== */

// Add a user-defined method callback (server side)
static int rpc_method_add_callback(rpc_connection_t *connection, const rpc_method_descriptor_t *desc)
{
  const int N_ENTRIES_ALLOC = 8;
  int i;

  // pre-allocate up to N_ENTRIES_ALLOC entries
  if (connection->callbacks == NULL) {
	if ((connection->callbacks = (rpc_method_descriptor_t *)calloc(N_ENTRIES_ALLOC, sizeof(connection->callbacks[0]))) == NULL)
	  return RPC_ERROR_NO_MEMORY;
	connection->n_callbacks = N_ENTRIES_ALLOC;
  }

  // look for a free slot
  for (i = connection->n_callbacks - 1; i >= 0; i--) {
	if (connection->callbacks[i].callback == NULL)
	  break;
  }

  // none found, reallocate
  if (i < 0) {
	if ((connection->callbacks = (rpc_method_descriptor_t *)realloc(connection->callbacks, (connection->n_callbacks + N_ENTRIES_ALLOC) * sizeof(connection->callbacks[0]))) == NULL)
	  return RPC_ERROR_NO_MEMORY;
	i = connection->n_callbacks;
	memset(&connection->callbacks[i], 0, N_ENTRIES_ALLOC * sizeof(connection->callbacks[0]));
	connection->n_callbacks += N_ENTRIES_ALLOC;
  }

  D(bug("rpc_method_add_callback for method %d in slot %d\n", desc->id, i));
  connection->callbacks[i] = *desc;
  return RPC_ERROR_NO_ERROR;
}

// Add user-defined method callbacks (server side)
int rpc_method_add_callbacks(rpc_connection_t *connection, const rpc_method_descriptor_t *descs, int n_descs)
{
  D(bug("rpc_method_add_callbacks\n"));

  if (connection == NULL)
	return RPC_ERROR_CONNECTION_NULL;
  if (connection->type != RPC_CONNECTION_SERVER)
	return RPC_ERROR_CONNECTION_TYPE_MISMATCH;

  while (--n_descs >= 0) {
	int error = rpc_method_add_callback(connection, &descs[n_descs]);
	if (error != RPC_ERROR_NO_ERROR)
	  return error;
  }

  return RPC_ERROR_NO_ERROR;
}

// Remove a user-defined method callback (common code)
int rpc_method_remove_callback_id(rpc_connection_t *connection, int id)
{
  D(bug("rpc_method_remove_callback_id\n"));

  if (connection->callbacks) {
	int i;
	for (i = 0; i < connection->n_callbacks; i++) {
	  if (connection->callbacks[i].id == id) {
		connection->callbacks[i].callback = NULL;
		return RPC_ERROR_NO_ERROR;
	  }
	}
  }

  return RPC_ERROR_GENERIC;
}

// Remove user-defined method callbacks (server side)
int rpc_method_remove_callbacks(rpc_connection_t *connection, const rpc_method_descriptor_t *callbacks, int n_callbacks)
{
  D(bug("rpc_method_remove_callbacks\n"));

  if (connection == NULL)
	return RPC_ERROR_CONNECTION_NULL;
  if (connection->type != RPC_CONNECTION_SERVER)
	return RPC_ERROR_CONNECTION_TYPE_MISMATCH;

  while (--n_callbacks >= 0) {
	int error = rpc_method_remove_callback_id(connection, callbacks[n_callbacks].id);
	if (error != RPC_ERROR_NO_ERROR)
	  return error;
  }

  return RPC_ERROR_NO_ERROR;
}


/* ====================================================================== */
/* === Remote Procedure Call (method invocation)                      === */
/* ====================================================================== */

// Invoke remote procedure (client side)
int rpc_method_invoke(rpc_connection_t *connection, int method, ...)
{
  D(bug("rpc_method_invoke method=%d\n", method));

  rpc_message_t message;
  int error;
  va_list args;

  if (connection == NULL)
	return RPC_ERROR_CONNECTION_NULL;
  if (connection->type != RPC_CONNECTION_CLIENT)
	return RPC_ERROR_CONNECTION_TYPE_MISMATCH;

  rpc_message_init(&message, connection);
  error = rpc_message_send_int32(&message, RPC_MESSAGE_START);
  if (error != RPC_ERROR_NO_ERROR)
	return error;
  error = rpc_message_send_int32(&message, method);
  if (error != RPC_ERROR_NO_ERROR)
	return error;
  va_start(args, method);
  error = rpc_message_send_args(&message, args);
  va_end(args);
  if (error != RPC_ERROR_NO_ERROR)
	return error;
  error = rpc_message_send_int32(&message, RPC_MESSAGE_END);
  if (error != RPC_ERROR_NO_ERROR)
	return error;
  error = rpc_message_flush(&message);
  if (error != RPC_ERROR_NO_ERROR)
	return error;
  return RPC_ERROR_NO_ERROR;
}

// Retrieve procedure arguments (server side)
int rpc_method_get_args(rpc_connection_t *connection, ...)
{
  D(bug("rpc_method_get_args\n"));

  int error;
  va_list args;
  rpc_message_t message;

  if (connection == NULL)
	return RPC_ERROR_CONNECTION_NULL;
  if (connection->type != RPC_CONNECTION_SERVER)
	return RPC_ERROR_CONNECTION_TYPE_MISMATCH;

  rpc_message_init(&message, connection);
  va_start(args, connection);
  error = rpc_message_recv_args(&message, args);
  va_end(args);

  return error;
}

// Wait for a reply from the remote procedure (client side)
int rpc_method_wait_for_reply(rpc_connection_t *connection, ...)
{
  D(bug("rpc_method_wait_for_reply\n"));

  int error, type;
  int32_t ret;
  va_list args;
  rpc_message_t message;

  if (connection == NULL)
	return RPC_ERROR_CONNECTION_NULL;
  if (connection->type != RPC_CONNECTION_CLIENT)
	return RPC_ERROR_CONNECTION_TYPE_MISMATCH;

  rpc_connection_set_status(connection, RPC_STATUS_BUSY);

  rpc_message_init(&message, connection);
  va_start(args, connection);
  type = va_arg(args, int);
  va_end(args);

  if (type != RPC_TYPE_INVALID) {
	error = rpc_message_recv_int32(&message, &ret);
	if (error != RPC_ERROR_NO_ERROR)
	  return_error(error);
	if (ret != RPC_MESSAGE_REPLY) {
	  D(bug("TRUNCATED 1 [%d]\n", ret));
	  return_error(RPC_ERROR_MESSAGE_TRUNCATED);
	}
	va_start(args, connection);
	error = rpc_message_recv_args(&message, args);
	va_end(args);
	if (error != RPC_ERROR_NO_ERROR)
	  return_error(error);
	error = rpc_message_recv_int32(&message, &ret);
	if (error != RPC_ERROR_NO_ERROR)
	  return_error(error);
	if (ret != RPC_MESSAGE_END) {
	  D(bug("TRUNCATED 2 [%d]\n", ret));
	  return_error(RPC_ERROR_MESSAGE_TRUNCATED);
	}
  }

  error = rpc_message_recv_int32(&message, &ret);
  if (error != RPC_ERROR_NO_ERROR)
	return_error(error);
  if (ret != RPC_MESSAGE_ACK) {
	D(bug("TRUNCATED 3 [%d]\n", ret));
	return_error(RPC_ERROR_MESSAGE_TRUNCATED);
  }

  return_error(RPC_ERROR_NO_ERROR);

 do_return:
  rpc_connection_set_status(connection, RPC_STATUS_IDLE);
  return error;
}

// Send a reply to the client (server side)
int rpc_method_send_reply(rpc_connection_t *connection, ...)
{
  D(bug("rpc_method_send_reply\n"));

  rpc_message_t message;
  int error;
  va_list args;

  if (connection == NULL)
	return RPC_ERROR_GENERIC;
  if (connection->type != RPC_CONNECTION_SERVER)
	return RPC_ERROR_GENERIC;

  rpc_message_init(&message, connection);
  error = rpc_message_send_int32(&message, RPC_MESSAGE_REPLY);
  if (error != RPC_ERROR_NO_ERROR)
	return error;
  va_start(args, connection);
  error = rpc_message_send_args(&message, args);
  va_end(args);
  if (error != RPC_ERROR_NO_ERROR)
	return error;
  error = rpc_message_send_int32(&message, RPC_MESSAGE_END);
  if (error != RPC_ERROR_NO_ERROR)
	return error;
  error = rpc_message_flush(&message);
  if (error != RPC_ERROR_NO_ERROR)
	return error;
  return RPC_ERROR_NO_ERROR;
}
