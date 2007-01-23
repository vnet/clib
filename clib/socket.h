/*
  Copyright (c) 2001, 2002, 2003 Eliot Dresselhaus

  Permission is hereby granted, free of charge, to any person obtaining
  a copy of this software and associated documentation files (the
  "Software"), to deal in the Software without restriction, including
  without limitation the rights to use, copy, modify, merge, publish,
  distribute, sublicense, and/or sell copies of the Software, and to
  permit persons to whom the Software is furnished to do so, subject to
  the following conditions:

  The above copyright notice and this permission notice shall be
  included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
  LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
  OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
  WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#ifndef _clib_included_socket_h
#define _clib_included_socket_h

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <clib/clib.h>
#include <clib/error.h>
#include <clib/format.h>

typedef struct _socket_t {
  /* File descriptor. */
  i32 fd;

  /* Config string for socket HOST:PORT or just HOST. */
  char * config;

  u32 flags;
#define SOCKET_IS_SERVER (1 << 0)
#define SOCKET_IS_CLIENT (0 << 0)
#define SOCKET_NON_BLOCKING_CONNECT (1 << 1)

  /* Read returned end-of-file. */
#define SOCKET_RX_END_OF_FILE (1 << 2)

  /* Transmit buffer.  Holds data waiting to be written. */
  u8 * tx_buffer;

  /* Receive buffer.  Holds data read from socket. */
  u8 * rx_buffer;

  /* Peer socket we are connected to. */
  struct sockaddr_in peer;

  clib_error_t * (* write_func) (struct _socket_t * sock);
  clib_error_t * (* read_func) (struct _socket_t * sock, int min_bytes);
  clib_error_t * (* close_func) (struct _socket_t * sock);
  void * private_data;
} socket_t;

/* socket config format is host:port.
   Unspecified port causes a free one to be chosen starting
   from IPPORT_USERRESERVED (5000). */
clib_error_t *
socket_init (socket_t * socket);

clib_error_t * socket_accept (socket_t * server, socket_t * client);

static inline uword socket_is_server (socket_t * sock)
{ return (sock->flags & SOCKET_IS_SERVER) != 0; }

static inline uword socket_is_client (socket_t * s)
{ return ! socket_is_server (s); }

static inline int socket_rx_end_of_file (socket_t * s)
{ return s->flags & SOCKET_RX_END_OF_FILE; }

static inline void *
socket_tx_add (socket_t * s, int n_bytes)
{
  u8 * result;
  vec_add2 (s->tx_buffer, result, n_bytes);
  return result;
}

static inline void
socket_tx_add_va_formatted (socket_t * s, char * fmt, va_list * va)
{ s->tx_buffer = va_format (s->tx_buffer, fmt, va); }

static inline void
socket_tx_add_formatted (socket_t * s, char * fmt, ...)
{
  va_list va;
  va_start (va, fmt);
  socket_tx_add_va_formatted (s, fmt, &va);
  va_end (va);
}

static inline clib_error_t *
socket_tx (socket_t * s)
{ return s->write_func (s); }

static inline clib_error_t *
socket_rx (socket_t * s, int n_bytes)
{ return s->read_func (s, n_bytes); }

static inline void
socket_free (socket_t *s)
{
  vec_free (s->tx_buffer);
  vec_free (s->rx_buffer);
  if (clib_mem_is_heap_object (s->config))
    vec_free (s->config);
  memset (s, 0, sizeof (s[0]));
}

static inline clib_error_t *
socket_close (socket_t *sock)
{
  clib_error_t * err;
  err = (* sock->close_func) (sock);
  socket_free (sock);
  return err;
}

#endif /* _clib_included_socket_h */
