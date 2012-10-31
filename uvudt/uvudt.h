// Copyright tom zhou<zs68j2ee@gmail.com>, 2012.


#ifndef UVUDT_H
#define UVUDT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "uv.h"


#if defined(__unix__) || defined(__POSIX__) || defined(__APPLE__)
// Unix-like platform

/*
 * uv_udt_t is a subclass of uv_stream_t
 *
 * Represents a UDT stream or UDT server.
 */
#define UV_UDT_PRIVATE_FIELDS           \
    int udtfd;                          \
    int accepted_udtfd;                 \

#else
// Windows platform

typedef struct uv_udt_poll_s {          \
  UV_REQ_FIELDS                         \
  char udtdummy;                        \
  int udtflag;                          \
} uv_udt_poll_t;                        \


typedef struct uv_udt_accept_s {        \
  UV_REQ_FIELDS                         \
  SOCKET accept_socket;                 \
  int accept_udtfd;                     \
  char accept_buffer[sizeof(struct sockaddr_storage) * 2 + 32]; \
  struct uv_udt_accept_s* next_pending; \
} uv_udt_accept_t;                      \


/*
 * uv_udt_t is a subclass of uv_stream_t
 *
 * Represents a UDT stream or UDT server.
 */
#define UV_UDT_REQ_POLL        0x1
#define UV_UDT_REQ_READ        0x2
#define UV_UDT_REQ_WRITE       0x4
#define UV_UDT_REQ_ACCEPT      0x8
#define UV_UDT_REQ_CONNECT     0x10

// dedicated error poll request
#define UV_UDT_REQ_POLL_ERROR  0x100

// active poll flags
#define UV_UDT_REQ_POLL_ACTIVE 0x1000

#define UV_UDT_PRIVATE_FIELDS                                                   \
    int udtfd;                                                                  \
    int udtflag;                                                                \
    /* Tail of a single-linked circular queue of pending reqs. If the queue */  \
    /* is empty, tail_ is NULL. If there is only one item, */                   \
    /* tail_->next_req == tail_ */                                              \
    uv_req_t* pending_reqs_tail_udtwrite;                                       \
    uv_req_t* pending_reqs_tail_udtconnect;                                     \
    uv_req_t* pending_reqs_tail_udtaccept;                                      \
    uv_req_t  udtreq_poll;                                                      \
    uv_req_t  udtreq_read;                                                      \
    uv_req_t  udtreq_write;                                                     \
    uv_req_t  udtreq_accept;                                                    \
    uv_req_t  udtreq_connect;                                                   \
    uv_req_t  udtreq_poll_error;                                                \


// Android platform
// TBD...

// iOS platform
// TBD...
#endif


/*
 * uv_udt_t is a subclass of uv_stream_t.
 * overrided stream methods
 */
UV_EXTERN int uv_udt_listen(uv_stream_t* stream, int backlog, uv_connection_cb cb);

UV_EXTERN int uv_udt_accept(uv_stream_t* server, uv_stream_t* client);

UV_EXTERN int uv_udt_read_start(uv_stream_t*, uv_alloc_cb alloc_cb,
    uv_read_cb read_cb);

UV_EXTERN int uv_udt_read_stop(uv_stream_t*);

UV_EXTERN int uv_udt_write(uv_write_t* req, uv_stream_t* handle,
    uv_buf_t bufs[], int bufcnt, uv_write_cb cb);

UV_EXTERN int uv_udt_is_readable(const uv_stream_t* handle);
UV_EXTERN int uv_udt_is_writable(const uv_stream_t* handle);

UV_EXTERN int uv_is_closing(const uv_handle_t* handle);

UV_EXTERN int uv_udt_shutdown(uv_shutdown_t* req, uv_stream_t* handle,
    uv_shutdown_cb cb);

UV_EXTERN void uv_udt_close(uv_handle_t* handle, uv_close_cb close_cb);


// UDT handle type
typedef struct uv_udt_s {
  UV_HANDLE_FIELDS
  UV_STREAM_FIELDS
  UV_TCP_PRIVATE_FIELDS
  UV_UDT_PRIVATE_FIELDS
  uv_poll_t uvpoll;
} uv_udt_t;


// UDT methods
UV_EXTERN int uv_udt_init(uv_loop_t*, uv_udt_t* handle);

UV_EXTERN int uv_udt_nodelay(uv_udt_t* handle, int enable);

UV_EXTERN int uv_udt_keepalive(uv_udt_t* handle, int enable,
    unsigned int delay);

UV_EXTERN int uv_udt_bind(uv_udt_t* handle, struct sockaddr_in);

UV_EXTERN int uv_udt_bind6(uv_udt_t* handle, struct sockaddr_in6);

UV_EXTERN int uv_udt_getsockname(uv_udt_t* handle, struct sockaddr* name,
    int* namelen);

UV_EXTERN int uv_udt_getpeername(uv_udt_t* handle, struct sockaddr* name,
    int* namelen);

UV_EXTERN int uv_udt_connect(uv_connect_t* req, uv_udt_t* handle,
    struct sockaddr_in address, uv_connect_cb cb);

UV_EXTERN int uv_udt_connect6(uv_connect_t* req, uv_udt_t* handle,
    struct sockaddr_in6 address, uv_connect_cb cb);

/* Enable/disable UDT socket in rendezvous mode */
UV_EXTERN int uv_udt_setrendez(uv_udt_t* handle, int enable);

/* binding udt socket on existing udp socket/fd */
UV_EXTERN int uv_udt_bindfd(uv_udt_t* handle, uv_syssocket_t udpfd);


/* Don't export the private CPP symbols. */
#undef UV_UDT_PRIVATE_FIELDS


#ifdef __cplusplus
}
#endif

#endif /* UVUDT_H */
