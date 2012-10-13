// Copyright tom zhou<zs68j2ee@gmail.com>, 2012.

#include <assert.h>

#include "uv.h"
#include "internal.h"
#include "handle-inl.h"
#include "stream-inl.h"
#include "req-inl.h"
#include "udtc.h" // udt head file

#define UDT_DEBUG 1

/*
 * Threshold of active udt streams for which to preallocate udt read buffers.
 * (Due to node slab allocator performing poorly under this pattern,
 *  the optimization is temporarily disabled (threshold=0).  This will be
 *  revisited once node allocator is improved.)
 */
static const unsigned int uv_active_udt_streams_threshold = 0;

/*
 * Number of simultaneous pending AcceptEx calls.
 */
static const unsigned int uv_udt_simultaneous_server_accepts = 1;

/* A one-byte-size buffer for use by uv_udt_read/write/accept/connect request */
static char uv_one_[] = {0x68};


static int udt__nonblock(int udtfd, int set)
{
    int block = (set ? 0 : 1);
    int rc1, rc2;

    rc1 = udt_setsockopt(udtfd, 0, (int)UDT_UDT_SNDSYN, (void *)&block, sizeof(block));
    rc2 = udt_setsockopt(udtfd, 0, (int)UDT_UDT_RCVSYN, (void *)&block, sizeof(block));

    return (rc1 | rc2);
}


static int uv__udt_nodelay(uv_udt_t* handle, SOCKET socket, int enable) {
  return 0;
}


static int uv__udt_keepalive(uv_udt_t* handle, SOCKET socket, int enable, unsigned int delay) {
	return 0;
}


static int uv_udt_set_socket(uv_loop_t* loop, uv_udt_t* handle,
    SOCKET socket, int imported) {
  ///int non_ifs_lsp = 1;

 ///printf("going on %s:%d\n", __FUNCTION__, __LINE__);
  assert(handle->socket == INVALID_SOCKET);

  /* Set the socket to nonblocking mode */
  if (udt__nonblock(handle->udtfd, 1) < 0) {
	 ///printf("going on %s:%d\n", __FUNCTION__, __LINE__);

    uv__set_sys_error(loop, uv_translate_udt_error());
    return -1;
  }

  /* Associate it with the I/O completion port. */
  /* Use uv_handle_t pointer as completion key. */
  if (CreateIoCompletionPort((HANDLE)socket,
                             loop->iocp,
                             (ULONG_PTR)handle,
                             0) == NULL) {
    if (imported) {
      handle->flags |= UV_HANDLE_EMULATE_IOCP;
	 ///printf("going on %s:%d\n", __FUNCTION__, __LINE__);
    } else {
	  printf("going on %s:%d\n", __FUNCTION__, __LINE__);
      uv__set_sys_error(loop, GetLastError());
      return -1;
    }
  }

#if 0
  non_ifs_lsp = (handle->flags & UV_HANDLE_IPV6) ? uv_tcp_non_ifs_lsp_ipv6 :
                                                   uv_tcp_non_ifs_lsp_ipv4;

  if (pSetFileCompletionNotificationModes && !non_ifs_lsp) {
    if (pSetFileCompletionNotificationModes((HANDLE) socket,
        FILE_SKIP_SET_EVENT_ON_HANDLE |
        FILE_SKIP_COMPLETION_PORT_ON_SUCCESS)) {
      handle->flags |= UV_HANDLE_SYNC_BYPASS_IOCP;
    } else if (GetLastError() != ERROR_INVALID_FUNCTION) {
      uv__set_sys_error(loop, GetLastError());
      return -1;
    }
  }
#endif

  if ((handle->flags & UV_HANDLE_TCP_NODELAY) &&
      uv__udt_nodelay(handle, socket, 1)) {
    return -1;
  }

  /* TODO: Use stored delay. */
  if ((handle->flags & UV_HANDLE_TCP_KEEPALIVE) &&
      uv__udt_keepalive(handle, socket, 1, 60)) {
    return -1;
  }

  handle->socket = socket;

 ///printf("going on %s:%d\n", __FUNCTION__, __LINE__);

  return 0;
}


int uv_udt_init(uv_loop_t* loop, uv_udt_t* handle) {
  uv_stream_init(loop, (uv_stream_t*) handle, UV_UDT);

  handle->accept_reqs = NULL;
  handle->pending_accepts = NULL;
  handle->socket = INVALID_SOCKET;
  handle->reqs_pending = 0;
  handle->func_acceptex = NULL;
  handle->func_connectex = NULL;
  handle->processed_accepts = 0;
  handle->udtfd = -1;

  loop->counters.udt_init++;

  return 0;
}


void uv_udt_endgame(uv_loop_t* loop, uv_udt_t* handle) {
  int status;
  int sys_error;
  unsigned int i;
  uv_tcp_accept_t* req;

 ///printf("going on %s:%d\n", __FUNCTION__, __LINE__);
  if (handle->flags & UV_HANDLE_CONNECTION &&
      handle->shutdown_req != NULL &&
      handle->write_reqs_pending == 0) {

    UNREGISTER_HANDLE_REQ(loop, handle, handle->shutdown_req);

    if (handle->flags & UV_HANDLE_CLOSING) {
      status = -1;
      sys_error = WSAEINTR;
    } else if (udt_close(handle->udtfd) == 0) {
      status = 0;
      handle->flags |= UV_HANDLE_SHUT;
    } else {
      status = -1;
      sys_error = uv_translate_udt_error();
    }

    if (handle->shutdown_req->cb) {
      if (status == -1) {
        uv__set_sys_error(loop, sys_error);
      }
      handle->shutdown_req->cb(handle->shutdown_req, status);
    }

    handle->shutdown_req = NULL;
    DECREASE_PENDING_REQ_COUNT(handle);
    return;
  }

  if (handle->flags & UV_HANDLE_CLOSING &&
      handle->reqs_pending == 0) {
    assert(!(handle->flags & UV_HANDLE_CLOSED));
    uv__handle_stop(handle);

    if (!(handle->flags & UV_HANDLE_TCP_SOCKET_CLOSED)) {
      udt_close(handle->udtfd);
      handle->flags |= UV_HANDLE_TCP_SOCKET_CLOSED;
    }

    if (!(handle->flags & UV_HANDLE_CONNECTION) && handle->accept_reqs) {
      if (handle->flags & UV_HANDLE_EMULATE_IOCP) {
        for (i = 0; i < uv_udt_simultaneous_server_accepts; i++) {
          req = &handle->accept_reqs[i];
          if (req->wait_handle != INVALID_HANDLE_VALUE) {
            UnregisterWait(req->wait_handle);
            req->wait_handle = INVALID_HANDLE_VALUE;
          }
          if (req->event_handle) {
            CloseHandle(req->event_handle);
            req->event_handle = NULL;
          }
        }
      }

      free(handle->accept_reqs);
      handle->accept_reqs = NULL;
    }

    if (handle->flags & UV_HANDLE_CONNECTION &&
        handle->flags & UV_HANDLE_EMULATE_IOCP) {
      if (handle->read_req.wait_handle != INVALID_HANDLE_VALUE) {
        UnregisterWait(handle->read_req.wait_handle);
        handle->read_req.wait_handle = INVALID_HANDLE_VALUE;
      }
      if (handle->read_req.event_handle) {
        CloseHandle(handle->read_req.event_handle);
        handle->read_req.event_handle = NULL;
      }
    }

    uv__handle_close(handle);
    loop->active_udt_streams--;
  }
}


static int uv__bind(uv_udt_t* handle,
                    int domain,
                    struct sockaddr* addr,
                    int addrsize) {
  DWORD err;
  int r;
  SOCKET sock;
  int optlen;

 ///printf("going on %s\n", __FUNCTION__);
  if (handle->socket == INVALID_SOCKET) {
    handle->udtfd = udt_socket(domain, SOCK_STREAM, 0);
    if (handle->udtfd < 0) {
      uv__set_sys_error(handle->loop, uv_translate_udt_error());
      return -1;
    }
	
    // fill Osfd
    assert(udt_getsockopt(handle->udtfd, 0, (int)UDT_UDT_OSFD, &sock, &optlen) == 0);

    if (uv_udt_set_socket(handle->loop, handle, sock, 0) == -1) {
      udt_close(handle->udtfd);
      return -1;
    }
  }

  r = udt_bind(handle->udtfd, addr, addrsize);

  if (r < 0) {
    err = uv_translate_udt_error();
    if (err == WSAEADDRINUSE) {
      /* Some errors are not to be reported until connect() or listen() */
      handle->bind_error = err;
      handle->flags |= UV_HANDLE_BIND_ERROR;
    } else {
      uv__set_sys_error(handle->loop, err);
      return -1;
    }
  }

  handle->flags |= UV_HANDLE_BOUND;

  return 0;
}


int uv__udt_bind(uv_udt_t* handle, struct sockaddr_in addr) {
  return uv__bind(handle,
                  AF_INET,
                  (struct sockaddr*)&addr,
                  sizeof(struct sockaddr_in));
}


int uv__udt_bind6(uv_udt_t* handle, struct sockaddr_in6 addr) {
  if (uv_allow_ipv6) {
    handle->flags |= UV_HANDLE_IPV6;
    return uv__bind(handle,
                    AF_INET6,
                    (struct sockaddr*)&addr,
                    sizeof(struct sockaddr_in6));

  } else {
    uv__set_sys_error(handle->loop, WSAEAFNOSUPPORT);
    return -1;
  }
}


static int uv__bindfd(
    	uv_udt_t* handle,
        SOCKET udpfd) {
  DWORD err;
  int r;
  SOCKET sock;
  int optlen;
  struct sockaddr_storage addr;
  socklen_t addrlen = sizeof(addr);
  int domain = AF_INET;

 ///printf("going on %s\n", __FUNCTION__);
  if (handle->socket == INVALID_SOCKET) {
    // extract domain info by existing udpfd ///////////////////////////////
    if (getsockname(udpfd, (struct sockaddr *)&addr, &addrlen) < 0) {
		uv__set_sys_error(handle->loop, WSAGetLastError());
	    return -1;
    }
    domain = addr.ss_family;
    ////////////////////////////////////////////////////////////////////////

    handle->udtfd = udt_socket(domain, SOCK_STREAM, 0);
    if (handle->udtfd < 0) {
      uv__set_sys_error(handle->loop, uv_translate_udt_error());
      return -1;
    }
	
    // fill Osfd
    assert(udt_getsockopt(handle->udtfd, 0, (int)UDT_UDT_OSFD, &sock, &optlen) == 0);

    if (uv_udt_set_socket(handle->loop, handle, sock, 0) == -1) {
      udt_close(handle->udtfd);
      return -1;
    }
  }

  r = udt_bind2(handle->udtfd, udpfd);

  if (r < 0) {
    err = uv_translate_udt_error();
    if (err == WSAEADDRINUSE) {
      /* Some errors are not to be reported until connect() or listen() */
      handle->bind_error = err;
      handle->flags |= UV_HANDLE_BIND_ERROR;
    } else {
      uv__set_sys_error(handle->loop, err);
      return -1;
    }
  }

  handle->flags |= UV_HANDLE_BOUND;

  return 0;
}

int uv__udt_bindfd(uv_udt_t* handle, uv_syssocket_t udpfd) {
    return uv__bindfd(handle, udpfd);
}


static void CALLBACK post_completion(void* context, BOOLEAN timed_out) {
  uv_req_t* req;
  uv_udt_t* handle;

 ///printf("going on %s\n", __FUNCTION__);

  req = (uv_req_t*) context;
  assert(req != NULL);
  handle = (uv_udt_t*)req->data;
  assert(handle != NULL);
  assert(!timed_out);

  if (!PostQueuedCompletionStatus(handle->loop->iocp,
                                  req->overlapped.InternalHigh,
                                  0,
                                  &req->overlapped)) {
    uv_fatal_error(GetLastError(), "PostQueuedCompletionStatus");
  }
}


static void CALLBACK post_write_completion(void* context, BOOLEAN timed_out) {
  uv_write_t* req;
  uv_udt_t* handle;

 ///printf("going on %s\n", __FUNCTION__);

  req = (uv_write_t*) context;
  assert(req != NULL);
  handle = (uv_udt_t*)req->handle;
  assert(handle != NULL);
  assert(!timed_out);

  if (!PostQueuedCompletionStatus(handle->loop->iocp,
                                  req->overlapped.InternalHigh,
                                  0,
                                  &req->overlapped)) {
    uv_fatal_error(GetLastError(), "PostQueuedCompletionStatus");
  }
}


static void uv_udt_queue_accept(uv_udt_t* handle, uv_tcp_accept_t* req) {
  uv_loop_t* loop = handle->loop;
  BOOL success;
  DWORD bytes;
  short family;
  uv_buf_t buf;
  DWORD flags;

 ///printf("going on %s:%d\n", __FUNCTION__, __LINE__);

  assert(handle->flags & UV_HANDLE_LISTENING);
  assert(req->accept_socket == INVALID_SOCKET);

  /* choose family and extension function */
  if (handle->flags & UV_HANDLE_IPV6) {
    family = AF_INET6;
  } else {
    family = AF_INET;
  }

#if 0
  /* Open a socket for the accepted connection. */
  accept_socket = socket(family, SOCK_STREAM, 0);
  if (accept_socket == INVALID_SOCKET) {
    SET_REQ_ERROR(req, WSAGetLastError());
    uv_insert_pending_req(loop, (uv_req_t*)req);
    handle->reqs_pending++;
    return;
  }

  /* Make the socket non-inheritable */
  if (!SetHandleInformation((HANDLE) accept_socket, HANDLE_FLAG_INHERIT, 0)) {
    SET_REQ_ERROR(req, GetLastError());
    uv_insert_pending_req(loop, (uv_req_t*)req);
    handle->reqs_pending++;
    closesocket(accept_socket);
    return;
  }
#endif

  /* Prepare the overlapped structure. */
  memset(&(req->overlapped), 0, sizeof(req->overlapped));

  if (handle->flags & UV_HANDLE_EMULATE_IOCP) {
    req->overlapped.hEvent = (HANDLE) ((ULONG_PTR) req->event_handle | 1);
  }

#if 0
  success = handle->func_acceptex(handle->socket,
                                  accept_socket,
                                  (void*)req->accept_buffer,
                                  0,
                                  sizeof(struct sockaddr_storage),
                                  sizeof(struct sockaddr_storage),
                                  &bytes,
                                  &req->overlapped);
#else
  // trigger a dummy read
  flags = 0;
  buf.base = (char*)uv_one_;
  buf.len = sizeof(uv_one_);

  success = WSARecv(handle->socket,
                   (WSABUF*)&buf,
                   1,
                   &bytes,
                   &flags,
                   &req->overlapped,
                   NULL);
#endif

 ///printf("going on %s:%d\n", __FUNCTION__, __LINE__);

  if (0/*UV_SUCCEEDED_WITHOUT_IOCP(success)*/) {
    /* Process the req without IOCP. */
    ///req->accept_socket = accept_socket;
    handle->reqs_pending++;
    uv_insert_pending_req(loop, (uv_req_t*)req);
	///printf("going on %s:%d\n", __FUNCTION__, __LINE__);
  } else if (UV_SUCCEEDED_WITH_IOCP(success)) {
    /* The req will be processed with IOCP. */
    ///req->accept_socket = accept_socket;
    handle->reqs_pending++;
    if (handle->flags & UV_HANDLE_EMULATE_IOCP &&
        req->wait_handle == INVALID_HANDLE_VALUE &&
        !RegisterWaitForSingleObject(&req->wait_handle,
          req->event_handle, post_completion, (void*) req,
          INFINITE, WT_EXECUTEINWAITTHREAD)) {
      SET_REQ_ERROR(req, GetLastError());
      uv_insert_pending_req(loop, (uv_req_t*)req);
      handle->reqs_pending++;
      return;
    }
	///printf("going on %s:%d\n", __FUNCTION__, __LINE__);
  } else {
    /* Make this req pending reporting an error. */
    SET_REQ_ERROR(req, WSAGetLastError());
    uv_insert_pending_req(loop, (uv_req_t*)req);
    handle->reqs_pending++;
    /* Destroy the preallocated client socket. */
    ///closesocket(accept_socket);
    /* Destroy the event handle */
    if (handle->flags & UV_HANDLE_EMULATE_IOCP) {
      CloseHandle(req->overlapped.hEvent);
      req->event_handle = NULL;
    }
	printf("going on %s:%d\n", __FUNCTION__, __LINE__);
  }
}


static void uv_udt_queue_read(uv_loop_t* loop, uv_udt_t* handle) {
  uv_read_t* req;
  uv_buf_t buf;
  int result;
  DWORD bytes, flags;

 ///printf("going on %s:%d\n", __FUNCTION__, __LINE__);

  assert(handle->flags & UV_HANDLE_READING);
  assert(!(handle->flags & UV_HANDLE_READ_PENDING));

  req = &handle->read_req;
  memset(&req->overlapped, 0, sizeof(req->overlapped));

  /*
   * Preallocate a read buffer if the number of active streams is below
   * the threshold.
  */
  if (loop->active_udt_streams < uv_active_udt_streams_threshold) {
	// never go here, tom tom
	assert(0);

    handle->flags &= ~UV_HANDLE_ZERO_READ;
    handle->read_buffer = handle->alloc_cb((uv_handle_t*) handle, 65536);
    assert(handle->read_buffer.len > 0);
    buf = handle->read_buffer;
  } else {
    handle->flags |= UV_HANDLE_ZERO_READ;
    buf.base = (char*)uv_one_;
    buf.len = sizeof(uv_one_);
  }

  /* Prepare the overlapped structure. */
  memset(&(req->overlapped), 0, sizeof(req->overlapped));

  if (handle->flags & UV_HANDLE_EMULATE_IOCP) {
    assert(req->event_handle);
    req->overlapped.hEvent = (HANDLE) ((ULONG_PTR) req->event_handle | 1);
  }

  flags = 0;
  result = WSARecv(handle->socket,
                   (WSABUF*)&buf,
                   1,
                   &bytes,
                   &flags,
                   &req->overlapped,
                   NULL);

 ///printf("going on %s:%d\n", __FUNCTION__, __LINE__);

  if (0/*UV_SUCCEEDED_WITHOUT_IOCP(result == 0)*/) {
    /* Process the req without IOCP. */
    handle->flags |= UV_HANDLE_READ_PENDING;
    req->overlapped.InternalHigh = bytes;
    handle->reqs_pending++;
    uv_insert_pending_req(loop, (uv_req_t*)req);
  } else if (UV_SUCCEEDED_WITH_IOCP(result == 0)) {
    /* The req will be processed with IOCP. */
    handle->flags |= UV_HANDLE_READ_PENDING;
    handle->reqs_pending++;
    if (handle->flags & UV_HANDLE_EMULATE_IOCP &&
        req->wait_handle == INVALID_HANDLE_VALUE &&
        !RegisterWaitForSingleObject(&req->wait_handle,
          req->event_handle, post_completion, (void*) req,
          INFINITE, WT_EXECUTEINWAITTHREAD)) {
      SET_REQ_ERROR(req, GetLastError());
      uv_insert_pending_req(loop, (uv_req_t*)req);
    }
  } else {
    /* Make this req pending reporting an error. */
    SET_REQ_ERROR(req, WSAGetLastError());
    uv_insert_pending_req(loop, (uv_req_t*)req);
    handle->reqs_pending++;
    printf("going on %s:%d\n", __FUNCTION__, __LINE__);
  }
}


int uv_udt_listen(uv_udt_t* handle, int backlog, uv_connection_cb cb) {
  uv_loop_t* loop = handle->loop;
  unsigned int i, simultaneous_accepts;
  uv_tcp_accept_t* req;

  assert(backlog > 0);

 ///printf("going on %s:%d\n", __FUNCTION__, __LINE__);

  if (handle->flags & UV_HANDLE_LISTENING) {
    handle->connection_cb = cb;
  }

  if (handle->flags & UV_HANDLE_READING) {
    uv__set_artificial_error(loop, UV_EISCONN);
    return -1;
  }

  if (handle->flags & UV_HANDLE_BIND_ERROR) {
    uv__set_sys_error(loop, handle->bind_error);
    return -1;
  }

  if (!(handle->flags & UV_HANDLE_BOUND) &&
      uv_udt_bind(handle, uv_addr_ip4_any_) < 0)
    return -1;

#if 0
  if (!handle->func_acceptex) {
    if(!uv_get_acceptex_function(handle->socket, &handle->func_acceptex)) {
      uv__set_sys_error(loop, WSAEAFNOSUPPORT);
      return -1;
    }
  }
#endif

  if (!(handle->flags & UV_HANDLE_SHARED_TCP_SOCKET) &&
      (udt_listen(handle->udtfd, backlog)) < 0) {
    uv__set_sys_error(loop, uv_translate_udt_error());
    return -1;
  }

  handle->flags |= UV_HANDLE_LISTENING;
  handle->connection_cb = cb;
  INCREASE_ACTIVE_COUNT(loop, handle);

  simultaneous_accepts = handle->flags & UV_HANDLE_TCP_SINGLE_ACCEPT ? 1
    : uv_udt_simultaneous_server_accepts;

  if(!handle->accept_reqs) {
    handle->accept_reqs = (uv_tcp_accept_t*)
      malloc(uv_udt_simultaneous_server_accepts * sizeof(uv_tcp_accept_t));
    if (!handle->accept_reqs) {
      uv_fatal_error(ERROR_OUTOFMEMORY, "malloc");
    }

    for (i = 0; i < simultaneous_accepts; i++) {
      req = &handle->accept_reqs[i];
      uv_req_init(loop, (uv_req_t*)req);
      req->type = UV_ACCEPT;
      req->accept_socket = INVALID_SOCKET;
      req->data = handle;

      req->wait_handle = INVALID_HANDLE_VALUE;
      if (handle->flags & UV_HANDLE_EMULATE_IOCP) {
        req->event_handle = CreateEvent(NULL, 0, 0, NULL);
        if (!req->event_handle) {
          uv_fatal_error(GetLastError(), "CreateEvent");
        }
      } else {
        req->event_handle = NULL;
      }

      uv_udt_queue_accept(handle, req);
    }

    /* Initialize other unused requests too, because uv_udt_endgame */
    /* doesn't know how how many requests were intialized, so it will */
    /* try to clean up {uv_udt_simultaneous_server_accepts} requests. */
    for (i = simultaneous_accepts; i < uv_udt_simultaneous_server_accepts; i++) {
      req = &handle->accept_reqs[i];
      uv_req_init(loop, (uv_req_t*) req);
      req->type = UV_ACCEPT;
      req->accept_socket = INVALID_SOCKET;
      req->data = handle;
      req->wait_handle = INVALID_HANDLE_VALUE;
    }
  }

  return 0;
}


int uv_udt_accept(uv_udt_t* server, uv_udt_t* client) {
  uv_loop_t* loop = server->loop;
  int rv = 0;
  struct sockaddr_storage saddr;
  int namelen = sizeof saddr;
  int optlen;
  char clienthost[NI_MAXHOST];
  char clientservice[NI_MAXSERV];
  uv_tcp_accept_t* req = server->pending_accepts;
  uv_buf_t buf;
  DWORD flags, bytes;
  BOOL success;
  
 ///printf("going on %s:%d\n", __FUNCTION__, __LINE__);

  if (!req) {
	 ///printf("going on %s:%d\n", __FUNCTION__, __LINE__);

    /* No valid connections found, so we error out. */
    uv__set_sys_error(loop, WSAEWOULDBLOCK);
    return -1;
  }
  ///printf("going on %s:%d\n", __FUNCTION__, __LINE__);

  // call udt accept
  ///////////////////////////////////////////////////////////////////////////////////////////
  req->accept_udtfd = udt_accept(server->udtfd, (struct sockaddr *)&saddr, &namelen);
  if (req->accept_udtfd < 0) {
	  if ((udt_getlasterror_code() == UDT_EASYNCRCV) ||
			  (udt_getlasterror_code() == UDT_ESECFAIL)) {
		  ///printf("going on %s:%d\n", __FUNCTION__, __LINE__);
		  uv__set_sys_error(loop, WSAEWOULDBLOCK);
		  req->accept_socket = INVALID_SOCKET;

		  // trigger a dummy read
		  {
			  flags = 0;
			  buf.base = (char*)uv_one_;
			  buf.len = sizeof(uv_one_);

			  success = WSARecv(
					  server->socket,
					  (WSABUF*)&buf,
					  1,
					  &bytes,
					  &flags,
					  &req->overlapped,
					  NULL);

			  if (UV_SUCCEEDED_WITH_IOCP(success)) {
				  /* The req will be processed with IOCP. */
				  ;
			  } else {
				  uv__set_sys_error(loop, WSAGetLastError());
				  return -1;
			  }
		  }

		  return 0;
	  } else {
		  ///printf("going on %s:%d\n", __FUNCTION__, __LINE__);
		  uv__set_sys_error(loop, WSAENOTCONN);
		  req->accept_socket = INVALID_SOCKET;
		  return -1;
	  }
  }
  client->udtfd = req->accept_udtfd;
  // fill Os fd
  assert(udt_getsockopt(client->udtfd, 0, (int)UDT_UDT_OSFD, &req->accept_socket, &optlen) == 0);

 ///printf("going on %s:%d, accept_socket:%d\n", __FUNCTION__, __LINE__, req->accept_socket);

  if (uv_udt_set_socket(client->loop, client, req->accept_socket, 0) == -1) {
	  udt_close(client->udtfd);
	  rv = -1;
  } else {
	  uv_connection_init((uv_stream_t*) client);
	  /* AcceptEx() implicitly binds the accepted socket. */
	  client->flags |= UV_HANDLE_BOUND;
	  
	  getnameinfo((struct sockaddr*)&saddr, sizeof saddr, clienthost, sizeof(clienthost), clientservice, sizeof(clientservice), NI_NUMERICHOST|NI_NUMERICSERV);
	  printf("new connection: %s:%s\n", clienthost, clientservice);
  }
  //////////////////////////////////////////////////////////////////////////////////////////////////

  /* Prepare the req to pick up a new connection */
  server->pending_accepts = req->next_pending;
  req->next_pending = NULL;
  req->accept_socket = INVALID_SOCKET;

  if (!(server->flags & UV_HANDLE_CLOSING)) {
    /* Check if we're in a middle of changing the number of pending accepts. */
    if (!(server->flags & UV_HANDLE_TCP_ACCEPT_STATE_CHANGING)) {
      uv_udt_queue_accept(server, req);
	 ///printf("going on %s:%d\n", __FUNCTION__, __LINE__);
    } else {
      /* We better be switching to a single pending accept. */
      assert(server->flags & UV_HANDLE_TCP_SINGLE_ACCEPT);

      server->processed_accepts++;

      if (server->processed_accepts >= uv_udt_simultaneous_server_accepts) {
        server->processed_accepts = 0;
        /*
         * All previously queued accept requests are now processed.
         * We now switch to queueing just a single accept.
         */
        uv_udt_queue_accept(server, &server->accept_reqs[0]);
        server->flags &= ~UV_HANDLE_TCP_ACCEPT_STATE_CHANGING;
        server->flags |= UV_HANDLE_TCP_SINGLE_ACCEPT;
		///printf("going on %s:%d\n", __FUNCTION__, __LINE__);
      }
    }
  }

  loop->active_udt_streams++;

  return rv;
}


int uv_udt_read_start(uv_udt_t* handle, uv_alloc_cb alloc_cb,
    uv_read_cb read_cb) {
  uv_loop_t* loop = handle->loop;

 ///printf("going on %s:%d\n", __FUNCTION__, __LINE__);

  if (!(handle->flags & UV_HANDLE_CONNECTION)) {
    uv__set_sys_error(loop, WSAEINVAL);
    return -1;
  }

 ///printf("going on %s:%d\n", __FUNCTION__, __LINE__);

  if (handle->flags & UV_HANDLE_READING) {
    uv__set_sys_error(loop, WSAEALREADY);
    return -1;
  }
  
 ///printf("going on %s:%d\n", __FUNCTION__, __LINE__);

  if (handle->flags & UV_HANDLE_EOF) {
    uv__set_sys_error(loop, WSAESHUTDOWN);
    return -1;
  }

 ///printf("going on %s:%d\n", __FUNCTION__, __LINE__);

  handle->flags |= UV_HANDLE_READING;
  handle->read_cb = read_cb;
  handle->alloc_cb = alloc_cb;
  INCREASE_ACTIVE_COUNT(loop, handle);

  /* If reading was stopped and then started again, there could still be a */
  /* read request pending. */
  if (!(handle->flags & UV_HANDLE_READ_PENDING)) {
    if (handle->flags & UV_HANDLE_EMULATE_IOCP &&
        !handle->read_req.event_handle) {
      handle->read_req.event_handle = CreateEvent(NULL, 0, 0, NULL);
      if (!handle->read_req.event_handle) {
        uv_fatal_error(GetLastError(), "CreateEvent");
      }
    }
    uv_udt_queue_read(loop, handle);

    ///printf("going on %s:%d\n", __FUNCTION__, __LINE__);
  }

  return 0;
}


int uv__udt_connect(uv_connect_t* req,
                    uv_udt_t* handle,
                    struct sockaddr_in address,
                    uv_connect_cb cb) {
  uv_loop_t* loop = handle->loop;
  int addrsize = sizeof(struct sockaddr_in);
  BOOL success;
  DWORD bytes, flags;
  uv_buf_t buf;

 ///printf("going on %s:%d\n", __FUNCTION__, __LINE__);

  if (handle->flags & UV_HANDLE_BIND_ERROR) {
    uv__set_sys_error(loop, handle->bind_error);
    return -1;
  }

  if (!(handle->flags & UV_HANDLE_BOUND) &&
      uv_udt_bind(handle, uv_addr_ip4_any_) < 0)
    return -1;

#if 0
  if (!handle->func_connectex) {
    if(!uv_get_connectex_function(handle->socket, &handle->func_connectex)) {
      uv__set_sys_error(loop, WSAEAFNOSUPPORT);
      return -1;
    }
  }
#endif

  uv_req_init(loop, (uv_req_t*) req);
  req->type = UV_CONNECT;
  req->handle = (uv_stream_t*) handle;
  req->cb = cb;
  memset(&req->overlapped, 0, sizeof(req->overlapped));

#if 0
  success = handle->func_connectex(handle->socket,
                                   (struct sockaddr*) &address,
                                   addrsize,
                                   NULL,
                                   0,
                                   &bytes,
                                   &req->overlapped);
#else
  // call udt connect
  udt_connect(handle->udtfd, (struct sockaddr*)&address, addrsize);

  if (UDT_CONNECTED == udt_getsockstate(handle->udtfd)) {
	  // insert request immediately
	  handle->reqs_pending++;
	  REGISTER_HANDLE_REQ(loop, handle, req);
	  uv_insert_pending_req(loop, (uv_req_t*)req);
  } else {
	  // trigger a dummy read
	  flags = 0;
	  buf.base = (char*)uv_one_;
	  buf.len = sizeof(uv_one_);

	  success = WSARecv(
			  handle->socket,
			  (WSABUF*)&buf,
			  1,
			  &bytes,
			  &flags,
			  &req->overlapped,
			  NULL);

	  ///printf("going on %s:%d\n", __FUNCTION__, __LINE__);

	  if (0/*UV_SUCCEEDED_WITHOUT_IOCP(success)*/) {
		  /* Process the req without IOCP. */
		  handle->reqs_pending++;
		  REGISTER_HANDLE_REQ(loop, handle, req);
		  uv_insert_pending_req(loop, (uv_req_t*)req);

		  ///printf("going on %s:%d\n", __FUNCTION__, __LINE__);
	  } else if (UV_SUCCEEDED_WITH_IOCP(success)) {
		  /* The req will be processed with IOCP. */
		  handle->reqs_pending++;
		  REGISTER_HANDLE_REQ(loop, handle, req);

		  ///printf("going on %s:%d\n", __FUNCTION__, __LINE__);
	  } else {
		  printf("going on %s:%d\n", __FUNCTION__, __LINE__);

		  uv__set_sys_error(loop, WSAGetLastError());
		  return -1;
	  }
  }
#endif

  return 0;
}


int uv__udt_connect6(uv_connect_t* req,
                     uv_udt_t* handle,
                     struct sockaddr_in6 address,
                     uv_connect_cb cb) {
  uv_loop_t* loop = handle->loop;
  int addrsize = sizeof(struct sockaddr_in6);
  BOOL success;
  DWORD bytes, flags;
  uv_buf_t buf;

  if (!uv_allow_ipv6) {
    uv__set_sys_error(loop, WSAEAFNOSUPPORT);
    return -1;
  }

  if (handle->flags & UV_HANDLE_BIND_ERROR) {
    uv__set_sys_error(loop, handle->bind_error);
    return -1;
  }

  if (!(handle->flags & UV_HANDLE_BOUND) &&
      uv_udt_bind6(handle, uv_addr_ip6_any_) < 0)
    return -1;

#if 0
  if (!handle->func_connectex) {
    if(!uv_get_connectex_function(handle->socket, &handle->func_connectex)) {
      uv__set_sys_error(loop, WSAEAFNOSUPPORT);
      return -1;
    }
  }
#endif

  uv_req_init(loop, (uv_req_t*) req);
  req->type = UV_CONNECT;
  req->handle = (uv_stream_t*) handle;
  req->cb = cb;
  memset(&req->overlapped, 0, sizeof(req->overlapped));

#if 0
  success = handle->func_connectex(handle->socket,
                                   (struct sockaddr*) &address,
                                   addrsize,
                                   NULL,
                                   0,
                                   &bytes,
                                   &req->overlapped);
#else
  // call udt connect
  udt_connect(handle->udtfd, (struct sockaddr*)&address, addrsize);

  if (UDT_CONNECTED == udt_getsockstate(handle->udtfd)) {
	  // insert request immediately
	  handle->reqs_pending++;
	  REGISTER_HANDLE_REQ(loop, handle, req);
	  uv_insert_pending_req(loop, (uv_req_t*)req);
  } else {
	  // trigger a dummy read
	  flags = 0;
	  buf.base = (char*)uv_one_;
	  buf.len = sizeof(uv_one_);

	  success = WSARecv(
			  handle->socket,
			  (WSABUF*)&buf,
			  1,
			  &bytes,
			  &flags,
			  &req->overlapped,
			  NULL);

	  ///printf("going on %s:%d\n", __FUNCTION__, __LINE__);

	  if (0/*UV_SUCCEEDED_WITHOUT_IOCP(success)*/) {
		  /* Process the req without IOCP. */
		  handle->reqs_pending++;
		  REGISTER_HANDLE_REQ(loop, handle, req);
		  uv_insert_pending_req(loop, (uv_req_t*)req);

		  ///printf("going on %s:%d\n", __FUNCTION__, __LINE__);
	  } else if (UV_SUCCEEDED_WITH_IOCP(success)) {
		  /* The req will be processed with IOCP. */
		  handle->reqs_pending++;
		  REGISTER_HANDLE_REQ(loop, handle, req);

		  ///printf("going on %s:%d\n", __FUNCTION__, __LINE__);
	  } else {
		  printf("going on %s:%d\n", __FUNCTION__, __LINE__);

		  uv__set_sys_error(loop, WSAGetLastError());
		  return -1;
	  }
  }
#endif

  return 0;
}


int uv_udt_getsockname(uv_udt_t* handle, struct sockaddr* name,
    int* namelen) {
  uv_loop_t* loop = handle->loop;
  int result;

 ///printf("going on %s:%d\n", __FUNCTION__, __LINE__);

  if (!(handle->flags & UV_HANDLE_BOUND)) {
    uv__set_sys_error(loop, WSAEINVAL);
    return -1;
  }

  if (handle->flags & UV_HANDLE_BIND_ERROR) {
    uv__set_sys_error(loop, handle->bind_error);
    return -1;
  }

  result = udt_getsockname(handle->udtfd, name, namelen);
  if (result != 0) {
    uv__set_sys_error(loop, uv_translate_udt_error());
    return -1;
  }

  return 0;
}


int uv_udt_getpeername(uv_udt_t* handle, struct sockaddr* name,
    int* namelen) {
  uv_loop_t* loop = handle->loop;
  int result;

 ///printf("going on %s:%d\n", __FUNCTION__, __LINE__);

  if (!(handle->flags & UV_HANDLE_BOUND)) {
    uv__set_sys_error(loop, WSAEINVAL);
    return -1;
  }

  if (handle->flags & UV_HANDLE_BIND_ERROR) {
    uv__set_sys_error(loop, handle->bind_error);
    return -1;
  }

  result = udt_getpeername(handle->udtfd, name, namelen);
  if (result != 0) {
    uv__set_sys_error(loop, uv_translate_udt_error());
    return -1;
  }

  return 0;
}


int uv_udt_write(uv_loop_t* loop, uv_write_t* req, uv_udt_t* handle,
    uv_buf_t bufs[], int bufcnt, uv_write_cb cb) {
  BOOL success;
  DWORD bytes, flags;
  uv_buf_t buf;
  int next, n, it;

 ///printf("going on %s:%d\n", __FUNCTION__, __LINE__);

  if (!(handle->flags & UV_HANDLE_CONNECTION)) {
    uv__set_sys_error(loop, WSAEINVAL);
    return -1;
  }

  if (handle->flags & UV_HANDLE_SHUTTING) {
    uv__set_sys_error(loop, WSAESHUTDOWN);
    return -1;
  }

  uv_req_init(loop, (uv_req_t*) req);
  req->type = UV_WRITE;
  req->handle = (uv_stream_t*) handle;
  req->cb = cb;

  // prepare bufs queue porting from unix/stream.c write2
  ////////////////////////////////////////////////////////////////////////////
  req->error = 0;

  if (bufcnt <= UV_REQ_BUFSML_SIZE)
	req->bufs = req->bufsml;
  else
	req->bufs = malloc(sizeof(uv_buf_t) * bufcnt);

  memcpy(req->bufs, bufs, bufcnt * sizeof(uv_buf_t));
  req->bufcnt = bufcnt;
  req->write_index = 0;
  req->queued_bytes = uv_count_bufs(bufs, bufcnt);
  ////////////////////////////////////////////////////////////////////////////

  /* Prepare the overlapped structure. */
  memset(&(req->overlapped), 0, sizeof(req->overlapped));

  if (handle->flags & UV_HANDLE_EMULATE_IOCP) {
    req->event_handle = CreateEvent(NULL, 0, 0, NULL);
    if (!req->event_handle) {
      uv_fatal_error(GetLastError(), "CreateEvent");
    }
    req->overlapped.hEvent = (HANDLE) ((ULONG_PTR) req->event_handle | 1);
    req->wait_handle = INVALID_HANDLE_VALUE;
  }

#if 0
  result = WSASend(handle->socket,
                   (WSABUF*)bufs,
                   bufcnt,
                   &bytes,
                   0,
                   &req->overlapped,
                   NULL);

  if (UV_SUCCEEDED_WITHOUT_IOCP(result == 0)) {
    /* Request completed immediately. */
    req->queued_bytes = 0;
    handle->reqs_pending++;
    handle->write_reqs_pending++;
    REGISTER_HANDLE_REQ(loop, handle, req);
    uv_insert_pending_req(loop, (uv_req_t*) req);
  } else if (UV_SUCCEEDED_WITH_IOCP(result == 0)) {
    /* Request queued by the kernel. */
    req->queued_bytes = uv_count_bufs(bufs, bufcnt);
    handle->reqs_pending++;
    handle->write_reqs_pending++;
    REGISTER_HANDLE_REQ(loop, handle, req);
    handle->write_queue_size += req->queued_bytes;
    if (handle->flags & UV_HANDLE_EMULATE_IOCP &&
        !RegisterWaitForSingleObject(&req->wait_handle,
          req->event_handle, post_write_completion, (void*) req,
          INFINITE, WT_EXECUTEINWAITTHREAD | WT_EXECUTEONLYONCE)) {
      SET_REQ_ERROR(req, GetLastError());
      uv_insert_pending_req(loop, (uv_req_t*)req);
    }
  } else {
    /* Send failed due to an error. */
    uv__set_sys_error(loop, uv_translate_udt_error());
    return -1;
  }
#else
  // 1.
  // try write on udt until got EAGAIN or send done
  next = 1;
  n = -1;
  for (it = 0; it < bufcnt; it ++) {
	  ULONG ilen = 0;

	  assert(bufs[it].len < 0x80000000); // avoid buf.len out of range
	  while (ilen < bufs[it].len) {
		  int rc = udt_send(handle->udtfd, bufs[it].base+ilen, bufs[it].len-ilen, 0);
		  if (rc < 0) {
			  next = 0;
			  break;
		  } else  {
			  if (n == -1) n = 0;
			  n += rc;
			  ilen += rc;
		  }
	  }
	  if (next == 0) break;
  }

  // 2. then queue write request with the rest of buffer
  if (n == -1) {
      // nothing to send done, queue request with iocp or check errors
	  if (udt_getlasterror_code() == UDT_EASYNCSND) {
		  // trigger a dummy read
		  flags = 0;
		  buf.base = (char*)uv_one_;
		  buf.len = sizeof(uv_one_);

		  success = WSARecv(
				  handle->socket,
				  (WSABUF*)&buf,
				  1,
				  &bytes,
				  &flags,
				  &req->overlapped,
				  NULL);

		  // queue request by iocp
		  if (UV_SUCCEEDED_WITH_IOCP(success)) {
			  handle->reqs_pending++;
			  handle->write_reqs_pending++;
			  REGISTER_HANDLE_REQ(loop, handle, req);
			  handle->write_queue_size += req->queued_bytes;
		  } else {
			  uv__set_sys_error(loop, WSAGetLastError());
			  return -1;
		  }
	  } else {
		  uv__set_sys_error(loop, uv_translate_udt_error());
		  return -1;
	  }
  } else {
	  assert(n <= req->queued_bytes);

	  if (n == req->queued_bytes) {
		  // send done, deliver request immediately

		  // queue request
		  req->queued_bytes = 0;
		  handle->reqs_pending++;
		  handle->write_reqs_pending++;
		  REGISTER_HANDLE_REQ(loop, handle, req);
		  uv_insert_pending_req(loop, (uv_req_t*) req);
	  } else {
          // partial send done, queue write request
		  req->queued_bytes -= n;
		  handle->write_queue_size += req->queued_bytes;

		  // update bufs
		  while (n > 0) {
			  uv_buf_t* buf = &(req->bufs[req->write_index]);
			  ULONG len = buf->len;

			  assert(req->write_index < req->bufcnt);

			  if ((ULONG)n < len) {
				  buf->base += n;
				  buf->len -= n;

				  n = 0;
			  } else {
				  /* Finished writing the buf at index req->write_index. */
				  req->write_index++;

				  n -= len;
			  }
		  }

		  // trigger a dummy read
		  flags = 0;
		  buf.base = (char*)uv_one_;
		  buf.len = sizeof(uv_one_);

		  success = WSARecv(
				  handle->socket,
				  (WSABUF*)&buf,
				  1,
				  &bytes,
				  &flags,
				  &req->overlapped,
				  NULL);

		  // queue request by iocp
		  if (UV_SUCCEEDED_WITH_IOCP(success)) {
			  handle->reqs_pending++;
			  handle->write_reqs_pending++;
			  REGISTER_HANDLE_REQ(loop, handle, req);

			 ///printf("going on %s:%d\n", __FUNCTION__, __LINE__);
		  } else {
			  uv__set_sys_error(loop, WSAGetLastError());

			 ///printf("going on %s:%d\n", __FUNCTION__, __LINE__);

			  return -1;
		  }
	  }
  }
#endif

  return 0;
}


void uv_process_udt_read_req(uv_loop_t* loop, uv_udt_t* handle,
		uv_req_t* req) {
	DWORD bytes, err, flags;
	uv_buf_t buf;
	BOOL success;
	int next, rcnt;
	
	///printf("going on %s:%d\n", __FUNCTION__, __LINE__);

	assert(handle->type == UV_UDT);

	// 0.
	// check if udt ready on read
	{
		int udtev, optlen;

		if (udt_getsockopt(handle->udtfd, 0, UDT_UDT_EVENT, &udtev, &optlen) < 0) {
			// check error anyway
			;
		} else {
			///printf("going on %s:%d with udt event:0x%x\n", __FUNCTION__, __LINE__, udtev);

			if (udtev & (UDT_UDT_EPOLL_IN | UDT_UDT_EPOLL_ERR)) {
				// go on
				;
			} else {
				// trigger dummy read !!!
				flags = 0;
		        buf.base = (char*)uv_one_;
		        buf.len = sizeof(uv_one_);

		        success = WSARecv(
					handle->socket,
				    (WSABUF*)&buf,
				    1,
				    &bytes,
				    &flags,
				    &req->overlapped,
				    NULL);

		        // queue request by iocp
		        if (UV_SUCCEEDED_WITH_IOCP(success)) {
			       ;///printf("going on %s:%d\n", __FUNCTION__, __LINE__);
		        } else {
			        uv__set_sys_error(loop, WSAGetLastError());

			       ///printf("going on %s:%d\n", __FUNCTION__, __LINE__);
		        }
				return;
			}
		}
	}

	handle->flags &= ~UV_HANDLE_READ_PENDING;

	if (!REQ_SUCCESS(req)) {
		/* An error occurred doing the read. */
		if ((handle->flags & UV_HANDLE_READING) ||
				!(handle->flags & UV_HANDLE_ZERO_READ)) {
			handle->flags &= ~UV_HANDLE_READING;
			DECREASE_ACTIVE_COUNT(loop, handle);
			buf = (handle->flags & UV_HANDLE_ZERO_READ) ?
					uv_buf_init(NULL, 0) : handle->read_buffer;

			err = GET_REQ_SOCK_ERROR(req);

			if (err == WSAECONNABORTED) {
				/*
				 * Turn WSAECONNABORTED into UV_ECONNRESET to be consistent with Unix.
				 */
				uv__set_error(loop, UV_ECONNRESET, err);
			} else {
				uv__set_sys_error(loop, err);
			}

			handle->read_cb((uv_stream_t*)handle, -1, buf);
		}
	} else {
		if (!(handle->flags & UV_HANDLE_ZERO_READ)) {
			/* The read was done with a non-zero buffer length. */
			if (req->overlapped.InternalHigh > 0) {
#if 0
				/* Successful read */
				handle->read_cb((uv_stream_t*)handle,
						req->overlapped.InternalHigh,
						handle->read_buffer);
				/* Read again only if bytes == buf.len */
				if (req->overlapped.InternalHigh < handle->read_buffer.len) {
					goto done;
				}
#else
				///printf("going on %s:%d\n", __FUNCTION__, __LINE__);

				// magic checking ...
				assert((req->overlapped.InternalHigh == 1) && (handle->read_buffer.base[0] == 0x68));
#endif
			} else {
				printf("going on %s:%d\n", __FUNCTION__, __LINE__);

				/* Connection closed */
				if (handle->flags & UV_HANDLE_READING) {
					handle->flags &= ~UV_HANDLE_READING;
					DECREASE_ACTIVE_COUNT(loop, handle);
				}
				handle->flags |= UV_HANDLE_EOF;

				uv__set_error(loop, UV_EOF, ERROR_SUCCESS);
				buf.base = 0;
				buf.len = 0;
				handle->read_cb((uv_stream_t*)handle, -1, handle->read_buffer);
				goto done;
			}
		}

		/* Do nonblocking reads until the buffer is empty */
#if 0
		while (handle->flags & UV_HANDLE_READING) {
			buf = handle->alloc_cb((uv_handle_t*) handle, 65536);
			assert(buf.len > 0);
			flags = 0;
			if (WSARecv(handle->socket,
					(WSABUF*)&buf,
					1,
					&bytes,
					&flags,
					NULL,
					NULL) != SOCKET_ERROR) {
				if (bytes > 0) {
					/* Successful read */
					handle->read_cb((uv_stream_t*)handle, bytes, buf);
					/* Read again only if bytes == buf.len */
					if (bytes < buf.len) {
						break;
					}
				} else {
					/* Connection closed */
					handle->flags &= ~UV_HANDLE_READING;
					DECREASE_ACTIVE_COUNT(loop, handle);
					handle->flags |= UV_HANDLE_EOF;

					uv__set_error(loop, UV_EOF, ERROR_SUCCESS);
					handle->read_cb((uv_stream_t*)handle, -1, buf);
					break;
				}
			} else {
				err = WSAGetLastError();
				if (err == WSAEWOULDBLOCK) {
					/* Read buffer was completely empty, report a 0-byte read. */
					uv__set_sys_error(loop, WSAEWOULDBLOCK);
					handle->read_cb((uv_stream_t*)handle, 0, buf);
				} else {
					/* Ouch! serious error. */
					handle->flags &= ~UV_HANDLE_READING;
					DECREASE_ACTIVE_COUNT(loop, handle);

					if (err == WSAECONNABORTED) {
						/* Turn WSAECONNABORTED into UV_ECONNRESET to be consistent with */
						/* Unix. */
						uv__set_error(loop, UV_ECONNRESET, err);
					} else {
						uv__set_sys_error(loop, err);
					}

					handle->read_cb((uv_stream_t*)handle, -1, buf);
				}
				break;
			}
		}
#else
		// read buf until got EAGAIN
		next = 1;
		while (handle->flags & UV_HANDLE_READING) {
			buf = handle->alloc_cb((uv_handle_t*) handle, 65536);
			assert(buf.len > 0);

			rcnt = 0;
			while (rcnt < buf.len) {
				bytes = udt_recv(handle->udtfd, buf.base+rcnt, buf.len-rcnt, 0);;
				if (bytes > 0) {
#if 0
					/* Successful read */
					handle->read_cb((uv_stream_t*)handle, bytes, buf);
					/* Read again only if bytes == buf.len */
					if (bytes < buf.len) {
						break;
					}
#else
					rcnt += bytes;
#endif
				} else {
					err = uv_translate_udt_error();
					if (err == WSAEWOULDBLOCK) {
						/* Read buffer was completely empty, report a 0-byte read. */
						if (rcnt == 0) uv__set_sys_error(loop, WSAEWOULDBLOCK);
						handle->read_cb((uv_stream_t*)handle, rcnt, buf);
					} else {
						/* Ouch! serious error. */
						handle->flags &= ~UV_HANDLE_READING;
						DECREASE_ACTIVE_COUNT(loop, handle);

						if (err == WSAECONNABORTED) {
							/* Turn WSAECONNABORTED into UV_ECONNRESET to be consistent with */
							/* Unix. */
							uv__set_error(loop, UV_ECONNRESET, err);
						} else {
							uv__set_sys_error(loop, err);
						}

						handle->read_cb((uv_stream_t*)handle, -1, buf);
					}
					next = 0;
					break;
				}
			}

			if (next == 1) {
				// going on
				handle->read_cb((uv_stream_t*)handle, buf.len, buf);
			} else {
				break;
			}
		}
#endif

done:
		/* Post another read if still reading and not closing. */
		if ((handle->flags & UV_HANDLE_READING) &&
			!(handle->flags & UV_HANDLE_READ_PENDING)) {
			uv_udt_queue_read(loop, handle);
			///printf("going on %s:%d\n", __FUNCTION__, __LINE__);
		}
	}

	DECREASE_PENDING_REQ_COUNT(handle);
}


void uv_process_udt_write_req(uv_loop_t* loop, uv_udt_t* handle,
		uv_write_t* req) {
	BOOL success;
	DWORD bytes, flags;
	uv_buf_t buf;
	uv_buf_t *bufs;
	int bufcnt;
	int next, n, it;

	///printf("going on %s:%d\n", __FUNCTION__, __LINE__);

	assert(handle->type == UV_UDT);

	assert(handle->write_queue_size >= req->queued_bytes);

	// 0.
	// check if udt ready on write
	{
		int udtev, optlen;

		if (udt_getsockopt(handle->udtfd, 0, UDT_UDT_EVENT, &udtev, &optlen) < 0) {
			// check error anyway
			;
		} else {
			///printf("going on %s:%d with udt event:0x%x\n", __FUNCTION__, __LINE__, udtev);

			if (udtev & (UDT_UDT_EPOLL_OUT | UDT_UDT_EPOLL_ERR)) {
				// go on
				;
			} else {
				// trigger dummy read !!!
				flags = 0;
		        buf.base = (char*)uv_one_;
		        buf.len = sizeof(uv_one_);

		        success = WSARecv(
					handle->socket,
				    (WSABUF*)&buf,
				    1,
				    &bytes,
				    &flags,
				    &req->overlapped,
				    NULL);

		        // queue request by iocp
		        if (UV_SUCCEEDED_WITH_IOCP(success)) {
			       ;///printf("going on %s:%d\n", __FUNCTION__, __LINE__);
		        } else {
			        uv__set_sys_error(loop, WSAGetLastError());

			       ///printf("going on %s:%d\n", __FUNCTION__, __LINE__);
		        }
				
				return;
			}
		}
	}

	// continue on write
	if (req->queued_bytes > 0) {
		///printf("going on %s:%d\n", __FUNCTION__, __LINE__);

		bufs = (uv_buf_t*) &(req->bufs[req->write_index]);
		bufcnt = req->bufcnt - req->write_index;

		// 1.
		// try write on udt until got EAGAIN or send done
		next = 1;
		n = -1;
		for (it = 0; it < bufcnt; it ++) {
			ULONG ilen = 0;

			assert(bufs[it].len < 0x80000000); // avoid buf.len out of range
			while (ilen < bufs[it].len) {
				int rc = udt_send(handle->udtfd, bufs[it].base+ilen, bufs[it].len-ilen, 0);
				if (rc < 0) {
					next = 0;
					break;
				} else  {
					if (n == -1) n = 0;
					n += rc;
					ilen += rc;
				}
			}
			if (next == 0) break;
		}

		// 2. then queue write request with the rest of buffer
		if (n == -1) {
			// nothing to send done, queue request with iocp or check errors
			if (udt_getlasterror_code() == UDT_EASYNCSND) {
				// trigger a dummy read
				flags = 0;
				buf.base = (char*)uv_one_;
				buf.len = sizeof(uv_one_);

				success = WSARecv(
						handle->socket,
						(WSABUF*)&buf,
						1,
						&bytes,
						&flags,
						&req->overlapped,
						NULL);

				// keep request by iocp
				if (UV_SUCCEEDED_WITH_IOCP(success)) {
					; // nothing to do
				} else {
					uv__set_sys_error(loop, WSAGetLastError());
					return;
				}
			} else {
				uv__set_sys_error(loop, uv_translate_udt_error());
				return;
			}
		} else {
			assert(n <= req->queued_bytes);

			if (n == req->queued_bytes) {
				// well done
				req->queued_bytes -= n;
				handle->write_queue_size -= n;
			} else {
				// partial done, trigger write request again
				req->queued_bytes -= n;
				handle->write_queue_size -= n;

				// update bufs
				while (n > 0) {
					uv_buf_t* buf = &(req->bufs[req->write_index]);
					ULONG len = buf->len;

					assert(req->write_index < req->bufcnt);

					if ((ULONG)n < len) {
						buf->base += n;
						buf->len -= n;

						n = 0;
					} else {
						/* Finished writing the buf at index req->write_index. */
						req->write_index++;

						n -= len;
					}
				}

				// trigger a dummy read
				flags = 0;
				buf.base = (char*)uv_one_;
				buf.len = sizeof(uv_one_);

				success = WSARecv(
						handle->socket,
						(WSABUF*)&buf,
						1,
						&bytes,
						&flags,
						&req->overlapped,
						NULL);

				// keep request by iocp
				if (UV_SUCCEEDED_WITH_IOCP(success)) {
					; // nothing to do
				} else {
					uv__set_sys_error(loop, WSAGetLastError());
					return;
				}
			}
		}
	}

	// in case all bufs send done
	if (req->queued_bytes == 0) {
		///printf("going on %s:%d\n", __FUNCTION__, __LINE__);

		// release in case allocated bufs
		if (req->bufcnt > UV_REQ_BUFSML_SIZE) {
			free(req->bufs);
			req->bufs = NULL;
			req->bufcnt = 0;
		}

		// 3.
		// if write done, then callback
		///handle->write_queue_size -= req->queued_bytes;

		UNREGISTER_HANDLE_REQ(loop, handle, req);

		if (handle->flags & UV_HANDLE_EMULATE_IOCP) {
			if (req->wait_handle != INVALID_HANDLE_VALUE) {
				UnregisterWait(req->wait_handle);
			}
			if (req->event_handle) {
				CloseHandle(req->event_handle);
			}
		}

		if (req->cb) {
			uv__set_sys_error(loop, GET_REQ_SOCK_ERROR(req));
			((uv_write_cb)req->cb)(req, loop->last_err.code == UV_OK ? 0 : -1);

			///printf("going on %s:%d\n", __FUNCTION__, __LINE__);
		}

		handle->write_reqs_pending--;
		if (handle->flags & UV_HANDLE_SHUTTING &&
		    handle->write_reqs_pending == 0) {
			uv_want_endgame(loop, (uv_handle_t*)handle);
		}

		DECREASE_PENDING_REQ_COUNT(handle);
	}
}


void uv_process_udt_accept_req(uv_loop_t* loop, uv_udt_t* handle,
		uv_req_t* raw_req) {
	DWORD flags, bytes;
	BOOL success;
	uv_buf_t buf;
	uv_tcp_accept_t* req = (uv_tcp_accept_t*) raw_req;

	assert(handle->type == UV_UDT);

	///printf("going on %s:%d\n", __FUNCTION__, __LINE__);

	// 0.
	// check if udt ready on read
	{
		int udtev, optlen;

		if (udt_getsockopt(handle->udtfd, 0, UDT_UDT_EVENT, &udtev, &optlen) < 0) {
			// check error anyway
			;
		} else {
			///printf("going on %s:%d with udt event:0x%x @%u\n", __FUNCTION__, __LINE__, udtev, handle->udtfd);

			if (udtev & (UDT_UDT_EPOLL_IN | UDT_UDT_EPOLL_ERR)) {
				// go on
				;
			} else {
				// trigger dummy read !!!
				flags = 0;
		        buf.base = (char*)uv_one_;
		        buf.len = sizeof(uv_one_);

		        success = WSARecv(
					handle->socket,
				    (WSABUF*)&buf,
				    1,
				    &bytes,
				    &flags,
				    &req->overlapped,
				    NULL);

		        // queue request by iocp
		        if (UV_SUCCEEDED_WITH_IOCP(success)) {
			       ;///printf("going on %s:%d\n", __FUNCTION__, __LINE__);
		        } else {
			        uv__set_sys_error(loop, WSAGetLastError());

			       ///printf("going on %s:%d\n", __FUNCTION__, __LINE__);
		        }
				return;
			}
		}
	}

	///printf("going on %s:%d\n", __FUNCTION__, __LINE__);

	/* If handle->accepted_socket is not a valid socket, then */
	/* uv_queue_accept must have failed. This is a serious error. We stop */
	/* accepting connections and report this error to the connection */
	/* callback. */
	if (0/*req->accept_socket == INVALID_SOCKET*/) {
		if (handle->flags & UV_HANDLE_LISTENING) {
			handle->flags &= ~UV_HANDLE_LISTENING;
			DECREASE_ACTIVE_COUNT(loop, handle);
			if (handle->connection_cb) {
				uv__set_sys_error(loop, GET_REQ_SOCK_ERROR(req));
				handle->connection_cb((uv_stream_t*)handle, -1);
			}
		}
	} else if (REQ_SUCCESS(req)/* &&
      setsockopt(req->accept_socket,
                  SOL_SOCKET,
                  SO_UPDATE_ACCEPT_CONTEXT,
                  (char*)&handle->socket,
                  sizeof(handle->socket)) == 0*/) {
		//printf("going on %s:%d\n", __FUNCTION__, __LINE__);

		req->next_pending = handle->pending_accepts;
		handle->pending_accepts = req;

		/* Accept and SO_UPDATE_ACCEPT_CONTEXT were successful. */
		if (handle->connection_cb) {
			handle->connection_cb((uv_stream_t*)handle, 0);
		}
	} else {
		/* Error related to accepted socket is ignored because the server */
		/* socket may still be healthy. If the server socket is broken
        /* uv_queue_accept will detect it. */
		///closesocket(req->accept_socket);
		req->accept_socket = INVALID_SOCKET;
		if (handle->flags & UV_HANDLE_LISTENING) {
			uv_udt_queue_accept(handle, req);
		}
		///printf("going on %s:%d\n", __FUNCTION__, __LINE__);
	}

	DECREASE_PENDING_REQ_COUNT(handle);
}


void uv_process_udt_connect_req(uv_loop_t* loop, uv_udt_t* handle,
    uv_connect_t* req) {
  DWORD flags, bytes;
  BOOL success;
  uv_buf_t buf;
  assert(handle->type == UV_UDT);

 ///printf("going on %s:%d\n", __FUNCTION__, __LINE__);

  // 0.
  // check if udt ready on write for connected done
  {
	  int udtev, optlen;

	  if (udt_getsockopt(handle->udtfd, 0, UDT_UDT_EVENT, &udtev, &optlen) < 0) {
		  // check error anyway
		  ;
	  } else {
		 ///printf("going on %s:%d with udt event:0x%x\n", __FUNCTION__, __LINE__, udtev);
		  
		  if (udtev & (UDT_UDT_EPOLL_OUT | UDT_UDT_EPOLL_ERR)) {
			  // go on
			  ;
		  } else {
				// trigger dummy read !!!
				flags = 0;
		        buf.base = (char*)uv_one_;
		        buf.len = sizeof(uv_one_);

		        success = WSARecv(
					handle->socket,
				    (WSABUF*)&buf,
				    1,
				    &bytes,
				    &flags,
				    &req->overlapped,
				    NULL);

		        // queue request by iocp
		        if (UV_SUCCEEDED_WITH_IOCP(success)) {
			       ;///printf("going on %s:%d\n", __FUNCTION__, __LINE__);
		        } else {
			        uv__set_sys_error(loop, WSAGetLastError());

			       ///printf("going on %s:%d\n", __FUNCTION__, __LINE__);
		        }
				return;
		  }
	  }
  }

 ///printf("going on %s:%d\n", __FUNCTION__, __LINE__);

  UNREGISTER_HANDLE_REQ(loop, handle, req);

  if (REQ_SUCCESS(req)) {
	 ///printf("going on %s:%d\n", __FUNCTION__, __LINE__);

	// check udt socket state
    if (UDT_CONNECTED == udt_getsockstate(handle->udtfd)
    		/*setsockopt(handle->socket,
                    SOL_SOCKET,
                    SO_UPDATE_CONNECT_CONTEXT,
                    NULL,
                    0) == 0*/) {
	  ///printf("going on %s:%d\n", __FUNCTION__, __LINE__);

      uv_connection_init((uv_stream_t*)handle);
      loop->active_udt_streams++;
      ((uv_connect_cb)req->cb)(req, 0);
    } else {
      ///printf("going on %s:%d\n", __FUNCTION__, __LINE__);

      uv__set_sys_error(loop, uv_translate_udt_error());
      ((uv_connect_cb)req->cb)(req, -1);
    }
  } else {
    uv__set_sys_error(loop, WSAGetLastError());
    ((uv_connect_cb)req->cb)(req, -1);
  }

  DECREASE_PENDING_REQ_COUNT(handle);
}


int uv_udt_nodelay(uv_udt_t* handle, int enable) {
  if (handle->socket != INVALID_SOCKET &&
      uv__udt_nodelay(handle, handle->socket, enable)) {
    return -1;
  }

  if (enable) {
    handle->flags |= UV_HANDLE_TCP_NODELAY;
  } else {
    handle->flags &= ~UV_HANDLE_TCP_NODELAY;
  }

  return 0;
}


int uv_udt_keepalive(uv_udt_t* handle, int enable, unsigned int delay) {
  if (handle->socket != INVALID_SOCKET &&
      uv__udt_keepalive(handle, handle->socket, enable, delay)) {
    return -1;
  }

  if (enable) {
    handle->flags |= UV_HANDLE_TCP_KEEPALIVE;
  } else {
    handle->flags &= ~UV_HANDLE_TCP_KEEPALIVE;
  }

  /* TODO: Store delay if handle->socket isn't created yet. */

  return 0;
}


int uv_udt_setrendez(uv_udt_t* handle, int enable) {
    int rndz = enable ? 1 : 0;
    
    if (handle->socket != INVALID_SOCKET &&
		udt_setsockopt(handle->udtfd, 0, UDT_UDT_RENDEZVOUS, &rndz, sizeof(rndz)))
	return -1;

	if (enable)
		handle->flags |= UV_HANDLE_UDT_RENDEZ;
	else
		handle->flags &= ~UV_HANDLE_UDT_RENDEZ;

	return 0;
}


int uv_udt_duplicate_socket(uv_udt_t* handle, int pid,
		LPWSAPROTOCOL_INFOW protocol_info) {
	///assert(0);
	printf("Not support uv_udt_duplicate_socket\n");
	return -1;
}


int uv_udt_simultaneous_accepts(uv_udt_t* handle, int enable) {
	///assert(0);
	printf("Not support uv_udt_simultaneous_accepts\n");
	return -1;
}


static int uv_udt_try_cancel_io(uv_udt_t* udt) {
	///assert(0);
	printf("Not support uv_udt_try_cancel_io\n");
	return -1;
}


void uv_udt_close(uv_loop_t* loop, uv_udt_t* udt) {
  int close_socket = 1;

 ///printf("going on %s:%d\n", __FUNCTION__, __LINE__);

  if (udt->flags & UV_HANDLE_READ_PENDING) {
    /* In order for winsock to do a graceful close there must not be any */
    /* any pending reads, or the socket must be shut down for writing */
    if (!(udt->flags & UV_HANDLE_SHARED_TCP_SOCKET)) {
      /* Just do shutdown on non-shared sockets, which ensures graceful close. */
      udt_close(udt->udtfd);
      udt->flags |= UV_HANDLE_SHUT;

    } else if (uv_udt_try_cancel_io(udt) == 0) {
      /* In case of a shared socket, we try to cancel all outstanding I/O, */
      /* If that works, don't close the socket yet - wait for the read req to */
      /* return and close the socket in uv_udt_endgame. */
      close_socket = 0;

    } else {
      /* When cancelling isn't possible - which could happen when an LSP is */
      /* present on an old Windows version, we will have to close the socket */
      /* with a read pending. That is not nice because trailing sent bytes */
      /* may not make it to the other side. */
    }

  } else if ((udt->flags & UV_HANDLE_SHARED_TCP_SOCKET) &&
              udt->accept_reqs != NULL) {
    /* Under normal circumstances closesocket() will ensure that all pending */
    /* accept reqs are canceled. However, when the socket is shared the */
    /* presence of another reference to the socket in another process will */
    /* keep the accept reqs going, so we have to ensure that these are */
    /* canceled. */
    if (uv_udt_try_cancel_io(udt) != 0) {
      /* When cancellation is not possible, there is another option: we can */
      /* close the incoming sockets, which will also cancel the accept */
      /* operations. However this is not cool because we might inadvertedly */
      /* close a socket that just accepted a new connection, which will */
      /* cause the connection to be aborted. */
      unsigned int i;
      for (i = 0; i < uv_udt_simultaneous_server_accepts; i++) {
        uv_tcp_accept_t* req = &udt->accept_reqs[i];
        if (req->accept_socket != INVALID_SOCKET &&
            !HasOverlappedIoCompleted(&req->overlapped)) {
          ///closesocket(req->accept_socket);
          udt_close(req->accept_udtfd);
          req->accept_socket = INVALID_SOCKET;
        }
      }
    }
  }

  if (udt->flags & UV_HANDLE_READING) {
    udt->flags &= ~UV_HANDLE_READING;
    DECREASE_ACTIVE_COUNT(loop, udt);
  }

  if (udt->flags & UV_HANDLE_LISTENING) {
    udt->flags &= ~UV_HANDLE_LISTENING;
    DECREASE_ACTIVE_COUNT(loop, udt);
  }

  if (close_socket) {
    ///closesocket(udt->socket);
	udt_close(udt->udtfd);
    udt->flags |= UV_HANDLE_TCP_SOCKET_CLOSED;
  }

  uv__handle_start(udt);

  if (udt->reqs_pending == 0) {
    uv_want_endgame(udt->loop, (uv_handle_t*)udt);
  }
}

// udt error translation to syserr in windows
/*
uv_err_code uv_translate_sys_error(int sys_errno) {
  switch (sys_errno) {
    case ERROR_SUCCESS:                     return UV_OK;
    case ERROR_BEGINNING_OF_MEDIA:          return UV_EIO;
    case ERROR_BUS_RESET:                   return UV_EIO;
    case ERROR_CRC:                         return UV_EIO;
    case ERROR_DEVICE_DOOR_OPEN:            return UV_EIO;
    case ERROR_DEVICE_REQUIRES_CLEANING:    return UV_EIO;
    case ERROR_DISK_CORRUPT:                return UV_EIO;
    case ERROR_EOM_OVERFLOW:                return UV_EIO;
    case ERROR_FILEMARK_DETECTED:           return UV_EIO;
    case ERROR_INVALID_BLOCK_LENGTH:        return UV_EIO;
    case ERROR_IO_DEVICE:                   return UV_EIO;
    case ERROR_NO_DATA_DETECTED:            return UV_EIO;
    case ERROR_NO_SIGNAL_SENT:              return UV_EIO;
    case ERROR_OPEN_FAILED:                 return UV_EIO;
    case ERROR_SETMARK_DETECTED:            return UV_EIO;
    case ERROR_SIGNAL_REFUSED:              return UV_EIO;
    case ERROR_FILE_NOT_FOUND:              return UV_ENOENT;
    case ERROR_INVALID_NAME:                return UV_ENOENT;
    case ERROR_INVALID_REPARSE_DATA:        return UV_ENOENT;
    case ERROR_MOD_NOT_FOUND:               return UV_ENOENT;
    case ERROR_PATH_NOT_FOUND:              return UV_ENOENT;
    case WSANO_DATA:                        return UV_ENOENT;
    case ERROR_ACCESS_DENIED:               return UV_EPERM;
    case ERROR_PRIVILEGE_NOT_HELD:          return UV_EPERM;
    case ERROR_NOACCESS:                    return UV_EACCES;
    case WSAEACCES:                         return UV_EACCES;
    case ERROR_ADDRESS_ALREADY_ASSOCIATED:  return UV_EADDRINUSE;
    case WSAEADDRINUSE:                     return UV_EADDRINUSE;
    case WSAEADDRNOTAVAIL:                  return UV_EADDRNOTAVAIL;
    case WSAEAFNOSUPPORT:                   return UV_EAFNOSUPPORT;
    case WSAEWOULDBLOCK:                    return UV_EAGAIN;
    case WSAEALREADY:                       return UV_EALREADY;
    case ERROR_LOCK_VIOLATION:              return UV_EBUSY;
    case ERROR_SHARING_VIOLATION:           return UV_EBUSY;
    case ERROR_CONNECTION_ABORTED:          return UV_ECONNABORTED;
    case WSAECONNABORTED:                   return UV_ECONNABORTED;
    case ERROR_CONNECTION_REFUSED:          return UV_ECONNREFUSED;
    case WSAECONNREFUSED:                   return UV_ECONNREFUSED;
    case ERROR_NETNAME_DELETED:             return UV_ECONNRESET;
    case WSAECONNRESET:                     return UV_ECONNRESET;
    case ERROR_ALREADY_EXISTS:              return UV_EEXIST;
    case ERROR_FILE_EXISTS:                 return UV_EEXIST;
    case WSAEFAULT:                         return UV_EFAULT;
    case ERROR_HOST_UNREACHABLE:            return UV_EHOSTUNREACH;
    case WSAEHOSTUNREACH:                   return UV_EHOSTUNREACH;
    case ERROR_OPERATION_ABORTED:           return UV_EINTR;
    case WSAEINTR:                          return UV_EINTR;
    case ERROR_INVALID_DATA:                return UV_EINVAL;
    case ERROR_SYMLINK_NOT_SUPPORTED:       return UV_EINVAL;
    case WSAEINVAL:                         return UV_EINVAL;
    case ERROR_CANT_RESOLVE_FILENAME:       return UV_ELOOP;
    case ERROR_TOO_MANY_OPEN_FILES:         return UV_EMFILE;
    case WSAEMFILE:                         return UV_EMFILE;
    case WSAEMSGSIZE:                       return UV_EMSGSIZE;
    case ERROR_FILENAME_EXCED_RANGE:        return UV_ENAMETOOLONG;
    case ERROR_NETWORK_UNREACHABLE:         return UV_ENETUNREACH;
    case WSAENETUNREACH:                    return UV_ENETUNREACH;
    case WSAENOBUFS:                        return UV_ENOBUFS;
    case ERROR_OUTOFMEMORY:                 return UV_ENOMEM;
    case ERROR_CANNOT_MAKE:                 return UV_ENOSPC;
    case ERROR_DISK_FULL:                   return UV_ENOSPC;
    case ERROR_EA_TABLE_FULL:               return UV_ENOSPC;
    case ERROR_END_OF_MEDIA:                return UV_ENOSPC;
    case ERROR_HANDLE_DISK_FULL:            return UV_ENOSPC;
    case ERROR_WRITE_PROTECT:               return UV_EROFS;
    case ERROR_NOT_CONNECTED:               return UV_ENOTCONN;
    case WSAENOTCONN:                       return UV_ENOTCONN;
    case ERROR_DIR_NOT_EMPTY:               return UV_ENOTEMPTY;
    case ERROR_NOT_SUPPORTED:               return UV_ENOTSUP;
    case ERROR_INSUFFICIENT_BUFFER:         return UV_EINVAL;
    case ERROR_INVALID_FLAGS:               return UV_EBADF;
    case ERROR_INVALID_HANDLE:              return UV_EBADF;
    case ERROR_INVALID_PARAMETER:           return UV_EINVAL;
    case ERROR_NO_UNICODE_TRANSLATION:      return UV_ECHARSET;
    case ERROR_BROKEN_PIPE:                 return UV_EOF;
    case ERROR_BAD_PIPE:                    return UV_EPIPE;
    case ERROR_NO_DATA:                     return UV_EPIPE;
    case ERROR_PIPE_NOT_CONNECTED:          return UV_EPIPE;
    case ERROR_PIPE_BUSY:                   return UV_EBUSY;
    case ERROR_SEM_TIMEOUT:                 return UV_ETIMEDOUT;
    case WSAETIMEDOUT:                      return UV_ETIMEDOUT;
    case WSAHOST_NOT_FOUND:                 return UV_ENOENT;
    case WSAENOTSOCK:                       return UV_ENOTSOCK;
    case ERROR_NOT_SAME_DEVICE:             return UV_EXDEV;
    default:                                return UV_UNKNOWN;
  }
}
*/

int uv_translate_udt_error() {
#ifdef UDT_DEBUG
	printf("func:%s, line:%d, errno: %d, %s\n", __FUNCTION__, __LINE__, udt_getlasterror_code(), udt_getlasterror_desc());
#endif

	switch (udt_getlasterror_code()) {
	case UDT_SUCCESS: return ERROR_SUCCESS;

	case UDT_EFILE: return ERROR_IO_DEVICE;

	case UDT_ERDPERM:
	case UDT_EWRPERM: return ERROR_ACCESS_DENIED;

	//case ENOSYS: return UV_ENOSYS;

	case UDT_ESOCKFAIL:
	case UDT_EINVSOCK: return WSAENOTSOCK;

	//case ENOENT: return UV_ENOENT;
	//case EACCES: return UV_EACCES;
	//case EAFNOSUPPORT: return UV_EAFNOSUPPORT;
	//case EBADF: return UV_EBADF;
	//case EPIPE: return UV_EPIPE;

	case UDT_EASYNCSND:
	case UDT_EASYNCRCV: return WSAEWOULDBLOCK;

	case UDT_ECONNSETUP:
	case UDT_ECONNFAIL: return WSAECONNRESET;

	//case EFAULT: return UV_EFAULT;
	//case EMFILE: return UV_EMFILE;

	case UDT_ELARGEMSG: return WSAEMSGSIZE;

	//case ENAMETOOLONG: return UV_ENAMETOOLONG;

	///case UDT_EINVSOCK: return EINVAL;

	//case ENETUNREACH: return UV_ENETUNREACH;

	case UDT_ECONNLOST: return WSAECONNABORTED;

	//case ELOOP: return UV_ELOOP;

	case UDT_ECONNREJ: return WSAECONNREFUSED;

	case UDT_EBOUNDSOCK: return WSAEADDRINUSE;

	//case EADDRNOTAVAIL: return UV_EADDRNOTAVAIL;
	//case ENOTDIR: return UV_ENOTDIR;
	//case EISDIR: return UV_EISDIR;
	case UDT_ENOCONN: return WSAENOTCONN;

	//case EEXIST: return UV_EEXIST;
	//case EHOSTUNREACH: return UV_EHOSTUNREACH;
	//case EAI_NONAME: return UV_ENOENT;
	//case ESRCH: return UV_ESRCH;

	case UDT_ETIMEOUT: return WSAETIMEDOUT;

	//case EXDEV: return UV_EXDEV;
	//case EBUSY: return UV_EBUSY;
	//case ENOTEMPTY: return UV_ENOTEMPTY;
	//case ENOSPC: return UV_ENOSPC;
	//case EROFS: return UV_EROFS;
	//case ENOMEM: return UV_ENOMEM;
	default: return -1;
	}
}

