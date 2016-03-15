// Copyright tom zhou<iwebpp@gmail.com>, 2012.

#include "uv.h"
#include "internal.h"
#include "udtc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>


/*
 case 0: return UV_OK;
 case EIO: return UV_EIO;
 case EPERM: return UV_EPERM;
 case ENOSYS: return UV_ENOSYS;
 case ENOTSOCK: return UV_ENOTSOCK;
 case ENOENT: return UV_ENOENT;
 case EACCES: return UV_EACCES;
 case EAFNOSUPPORT: return UV_EAFNOSUPPORT;
 case EBADF: return UV_EBADF;
 case EPIPE: return UV_EPIPE;
 case EAGAIN: return UV_EAGAIN;
 #if EWOULDBLOCK != EAGAIN
 case EWOULDBLOCK: return UV_EAGAIN;
 #endif
 case ECONNRESET: return UV_ECONNRESET;
 case EFAULT: return UV_EFAULT;
 case EMFILE: return UV_EMFILE;
 case EMSGSIZE: return UV_EMSGSIZE;
 case ENAMETOOLONG: return UV_ENAMETOOLONG;
 case EINVAL: return UV_EINVAL;
 case ENETUNREACH: return UV_ENETUNREACH;
 case ECONNABORTED: return UV_ECONNABORTED;
 case ELOOP: return UV_ELOOP;
 case ECONNREFUSED: return UV_ECONNREFUSED;
 case EADDRINUSE: return UV_EADDRINUSE;
 case EADDRNOTAVAIL: return UV_EADDRNOTAVAIL;
 case ENOTDIR: return UV_ENOTDIR;
 case EISDIR: return UV_EISDIR;
 case ENOTCONN: return UV_ENOTCONN;
 case EEXIST: return UV_EEXIST;
 case EHOSTUNREACH: return UV_EHOSTUNREACH;
 case EAI_NONAME: return UV_ENOENT;
 case ESRCH: return UV_ESRCH;
 case ETIMEDOUT: return UV_ETIMEDOUT;
 case EXDEV: return UV_EXDEV;
 case EBUSY: return UV_EBUSY;
 case ENOTEMPTY: return UV_ENOTEMPTY;
 case ENOSPC: return UV_ENOSPC;
 case EROFS: return UV_EROFS;
 case ENOMEM: return UV_ENOMEM;
 default: return UV_UNKNOWN;
 */

// transfer UDT error code to system errno
int uv_translate_udt_error() {
#ifdef UDT_DEBUG
    fprintf(stdout, "func:%s, line:%d, errno: %d, %s\n", __FUNCTION__, __LINE__, udt_getlasterror_code(), udt_getlasterror_desc());
#endif
    
    switch (udt_getlasterror_code()) {
        case UDT_SUCCESS: return errno = 0;
            
        case UDT_EFILE: return errno = EIO;
            
        case UDT_ERDPERM:
        case UDT_EWRPERM: return errno = EPERM;
            
            //case ENOSYS: return UV_ENOSYS;
            
        case UDT_ESOCKFAIL:
        case UDT_EINVSOCK: return errno = ENOTSOCK;
            
            //case ENOENT: return UV_ENOENT;
            //case EACCES: return UV_EACCES;
            //case EAFNOSUPPORT: return UV_EAFNOSUPPORT;
            //case EBADF: return UV_EBADF;
            //case EPIPE: return UV_EPIPE;
            
        case UDT_EASYNCSND:
        case UDT_EASYNCRCV: return errno = EAGAIN;
            
        case UDT_ECONNSETUP:
        case UDT_ECONNFAIL: return errno = ECONNRESET;
            
            //case EFAULT: return UV_EFAULT;
            //case EMFILE: return UV_EMFILE;
            
        case UDT_ELARGEMSG: return errno = EMSGSIZE;
            
            //case ENAMETOOLONG: return UV_ENAMETOOLONG;
            
            ///case UDT_EINVSOCK: return EINVAL;
            
            //case ENETUNREACH: return UV_ENETUNREACH;
            
            //case ERROR_BROKEN_PIPE: return UV_EOF;
        case UDT_ECONNLOST: return errno = EPIPE;
            
            //case ELOOP: return UV_ELOOP;
            
        case UDT_ECONNREJ: return errno = ECONNREFUSED;
            
        case UDT_EBOUNDSOCK: return errno = EADDRINUSE;
            
            //case EADDRNOTAVAIL: return UV_EADDRNOTAVAIL;
            //case ENOTDIR: return UV_ENOTDIR;
            //case EISDIR: return UV_EISDIR;
        case UDT_ENOCONN: return errno = ENOTCONN;
            
            //case EEXIST: return UV_EEXIST;
            //case EHOSTUNREACH: return UV_EHOSTUNREACH;
            //case EAI_NONAME: return UV_ENOENT;
            //case ESRCH: return UV_ESRCH;
            
        case UDT_ETIMEOUT: return errno = ETIMEDOUT;
            
            //case EXDEV: return UV_EXDEV;
            //case EBUSY: return UV_EBUSY;
            //case ENOTEMPTY: return UV_ENOTEMPTY;
            //case ENOSPC: return UV_ENOSPC;
            //case EROFS: return UV_EROFS;
            //case ENOMEM: return UV_ENOMEM;
        default: return errno = -1;
    }
}


///#define UDT_DEBUG 1
/*
int uv_udt_init(uv_loop_t* loop, uv_udt_t* udt) {
	static int _initialized = 0;

	// insure startup UDT
	if (_initialized == 0) {
		assert(udt_startup() == 0);
		_initialized = 1;
	}

	uv__stream_init(loop, (uv_stream_t*)udt, UV_UDT);
	udt->udtfd = udt->accepted_udtfd = -1;
	loop->counters.udt_init++;
	return 0;
}


static int maybe_new_socket(uv_udt_t* handle, int domain, int flags) {
	int optlen;

	if (handle->fd != -1)
		return 0;

	if ((handle->udtfd = udt__socket(domain, SOCK_STREAM, 0)) == -1) {
		return uv__set_sys_error(handle->loop, uv_translate_udt_error());
	}

	// fill Osfd
	assert(udt_getsockopt(handle->udtfd, 0, (int)UDT_UDT_OSFD, &handle->fd, &optlen) == 0);

	if (uv__stream_open((uv_stream_t*)handle, handle->fd, flags)) {
		udt_close(handle->udtfd);
		handle->fd = -1;
		return -1;
	}

	return 0;
}
*/

static int maybe_new_socket(uv_udt_t* handle, int domain, int flags) {
    int err;
    int optlen;
    
    if (domain == AF_UNSPEC || handle->io_watcher.fd != -1) {
        handle->flags |= flags;
        return 0;
    }
    err = udt__socket(domain, SOCK_STREAM, 0);
    if (err == -1) {
        return err;
    }
    
    handle->udtfd = err;
    
    
    // fill Osfd
    int error = udt_getsockopt(handle->udtfd, 0, (int)UDT_UDT_OSFD, &handle->io_watcher.fd, &optlen);
    assert(error == 0);
    
    err = uv__stream_open((uv_stream_t*)handle, handle->io_watcher.fd, flags);
    if (err) {
        udt_close(handle->udtfd);
        handle->io_watcher.fd = -1;
        return err;
    }

    
    
    return 0;
}


int uv_udt_init_ex(uv_loop_t* loop, uv_udt_t* udt, unsigned int flags) {
    int domain;
    
    /* Use the lower 8 bits for the domain */
    domain = flags & 0xFF;
    if (domain != AF_INET && domain != AF_INET6 && domain != AF_UNSPEC)
        return -EINVAL;
    
    if (flags & ~0xFF)
        return -EINVAL;
    
    uv__stream_init(loop, (uv_stream_t*)udt, UV_UDT);
    
    /* If anything fails beyond this point we need to remove the handle from
     * the handle queue, since it was added by uv__handle_init in uv_stream_init.
     */
    
    
    if (domain != AF_UNSPEC) {
        int err = maybe_new_socket(udt, domain, 0);
        if (err) {
            QUEUE_REMOVE(&udt->handle_queue);
            return err;
        }
        
        //from old version uv_udt_init()

    }
    udt->udtfd = udt->accepted_udtfd = -1;
    loop->counters.udt_init++;
    return 0;
}


int uv_udt_init(uv_loop_t* loop, uv_udt_t* udt) {
    return uv_udt_init_ex(loop, udt, AF_UNSPEC);
}

/*
 static int uv__bind(
 uv_udt_t* udt,
 int domain,
 struct sockaddr* addr,
 int addrsize)
 {
	int saved_errno;
	int status;
 
	saved_errno = errno;
	status = -1;
 
	if (maybe_new_socket(udt, domain, UV_STREAM_READABLE|UV_STREAM_WRITABLE))
 return -1;
 
	assert(udt->fd > 0);
 
	udt->delayed_error = 0;
	if (udt_bind(udt->udtfd, addr, addrsize) < 0) {
 if (udt_getlasterror_code() == UDT_EBOUNDSOCK) {
 udt->delayed_error = EADDRINUSE;
 } else {
 uv__set_sys_error(udt->loop, uv_translate_udt_error());
 goto out;
 }
	}
	status = 0;
 
 out:
	errno = saved_errno;
	return status;
 }

 
 */

static int uv__bind(
                    uv_udt_t* udt,
                    int domain,
                    const struct sockaddr* addr,
                    unsigned int addrlen,
                    unsigned int flags)
{
    int saved_errno;
    int status;
    
    saved_errno = errno;
    status = -1;
    
    if (maybe_new_socket(udt, domain, UV_STREAM_READABLE|UV_STREAM_WRITABLE))
        return -1;
    
    assert(udt->io_watcher.fd > 0);
    
    udt->delayed_error = 0;
    if (udt_bind(udt->udtfd, addr, addrlen) < 0) {
        if (udt_getlasterror_code() == UDT_EBOUNDSOCK) {
            udt->delayed_error = EADDRINUSE;
        } else {
            saved_errno =  uv_translate_udt_error();
            goto out;
        }
    }
    status = 0;
    
out:
    errno = saved_errno;
    return status;
}


int uv__udt_bind(uv_udt_t* udt,
                 const struct sockaddr* addr,
                 unsigned int addrlen,
                 unsigned int flags) {
    return uv__bind(udt,
                    AF_INET,
                    addr,
                    addrlen,
                    flags);
}




/*
int uv__udt_bind(uv_udt_t* udt,
                 const struct sockaddr* addr,
                 unsigned int addrlen,
                 unsigned int flags) {
    int err;
    int on;
    
    // Cannot set IPv6-only mode on non-IPv6 socket.
    if ((flags & UV_UDT_IPV6ONLY) && addr->sa_family != AF_INET6)
        return -EINVAL;
    
    err = maybe_new_socket(udt,
                           addr->sa_family,
                           UV_STREAM_READABLE | UV_STREAM_WRITABLE);
    if (err)
        return err;
    
    on = 1;
    
    //assert(uv__stream_fd(udt) > 0);
    
    
    //if (setsockopt(udt->udtfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)))
    //    return -errno;
    
#ifdef IPV6_V6ONLY
    if (addr->sa_family == AF_INET6) {
        on = (flags & UV_UDT_IPV6ONLY) != 0;
        if (setsockopt(udt->udtfd,
                       IPPROTO_IPV6,
                       IPV6_V6ONLY,
                       &on,
                       sizeof on) == -1) {
            return -errno;
        }
    }
#endif
    
    errno = 0;
    
    udt->delayed_error = errno;
    
    if (udt_bind(udt->udtfd, addr, addrlen) && errno != EADDRINUSE) {
        if (errno == EAFNOSUPPORT)

            return -EINVAL;
        return -errno;
    }
    
    

    udt->delayed_error = -errno;
    
    if (addr->sa_family == AF_INET6)
        udt->flags |= UV_HANDLE_IPV6;
    
    return 0;
}
*/

int uv__udt_connect(uv_connect_t* req,
                    uv_udt_t* handle,
                    const struct sockaddr* addr,
                    unsigned int addrlen,
                    uv_connect_cb cb) {
    int err;
    int r;
    
    assert(handle->type == UV_UDT);
    
    if (handle->connect_req != NULL)
        return -EALREADY;  /* FIXME(bnoordhuis) -EINVAL or maybe -EBUSY. */
    
    err = maybe_new_socket(handle,
                           addr->sa_family,
                           UV_STREAM_READABLE | UV_STREAM_WRITABLE);
    if (err)
        return err;
    
    handle->delayed_error = 0;
    
    r = udt_connect(((uv_udt_t *)handle)->udtfd, addr, addrlen);
    
    ///if (r < 0)
    {
        // checking connecting state first
        int state = udt_getsockstate(((uv_udt_t *)handle)->udtfd);
        
        if (UDT_CONNECTING == state) {
            ; /* not an error */
        } else {
            switch (udt_getlasterror_code()) {
                    /* If we get a ECONNREFUSED wait until the next tick to report the
                     * error. Solaris wants to report immediately--other unixes want to
                     * wait.
                     */
                case UDT_ECONNREJ:
                    handle->delayed_error = ECONNREFUSED;
                    break;
                    
                default:
                    return udt_getlasterror_code();
            }
        }
    }
    
    
    uv__req_init(handle->loop, req, UV_CONNECT);
    req->cb = cb;
    req->handle = (uv_stream_t*) handle;
    QUEUE_INIT(&req->queue);
    handle->connect_req = req;
    
    uv__io_start(handle->loop, &handle->io_watcher, UV__POLLOUT);
    
    if (handle->delayed_error)
        uv__io_feed(handle->loop, &handle->io_watcher);
    
    return 0;
}


/*
static int uv__connect(uv_connect_t* req,
                       uv_udt_t* handle,
                       struct sockaddr* addr,
                       socklen_t addrlen,
                       uv_connect_cb cb) {
  int r;

  assert(handle->type == UV_UDT);

  if (handle->connect_req)
    return uv__set_sys_error(handle->loop, EALREADY);

  if (maybe_new_socket(handle,
                       addr->sa_family,
                       UV_STREAM_READABLE|UV_STREAM_WRITABLE)) {
    return -1;
  }

  handle->delayed_error = 0;

  r = udt_connect(((uv_udt_t *)handle)->udtfd, addr, addrlen);

  if (r < 0) {
	  // checking connecting state first
	  if (UDT_CONNECTING == udt_getsockstate(((uv_udt_t *)handle)->udtfd)) {
		  ; //not an error
	  } else {
		  switch (udt_getlasterror_code()) {
		  //If we get a ECONNREFUSED wait until the next tick to report the
		   //error. Solaris wants to report immediately--other unixes want to
		  //wait.
		  //
		  case UDT_ECONNREJ:
			  handle->delayed_error = ECONNREFUSED;
			  break;

		  default:
			  return uv__set_sys_error(handle->loop, uv_translate_udt_error());
		  }
	  }
  }

  uv__req_init(handle->loop, req, UV_CONNECT);
  req->cb = cb;
  req->handle = (uv_stream_t*) handle;
  ngx_queue_init(&req->queue);
  handle->connect_req = req;

  uv__io_start(handle->loop, &handle->write_watcher);

  if (handle->delayed_error)
    uv__io_feed(handle->loop, &handle->write_watcher, UV__IO_READ);

  return 0;
}
*/


int uv_udt_open(uv_udt_t* handle, uv_os_sock_t sock) {
    int err;
    
    err = uv__nonblock(sock, 1);
    if (err)
        return err;
    
    return uv__stream_open((uv_stream_t*)handle,
                           sock,
                           UV_STREAM_READABLE | UV_STREAM_WRITABLE);
}

int uv_udt_getsockname(uv_udt_t* handle, struct sockaddr* name,
                       int* namelen) {
    int saved_errno;
    socklen_t socklen;
    
    /* Don't clobber errno. */
    saved_errno = errno;
    
    if (handle->delayed_error) {
        return handle->delayed_error;
    }
    
    if (handle->io_watcher.fd < 0) {
        return -EINVAL;
    }
    
    /* sizeof(socklen_t) != sizeof(int) on some systems. */
    socklen = (socklen_t) *namelen;
    
    if (udt_getsockname(handle->udtfd, name, namelen) == -1) {
        return -errno;
    }
    
    *namelen = (int) socklen;
    return 0;
}



int uv_udt_getpeername(uv_udt_t* handle,
                       struct sockaddr* name,
                       int* namelen) {
    socklen_t socklen;
    
    if (handle->delayed_error)
        return handle->delayed_error;
    
    if (handle->io_watcher.fd < 0)
        return -EINVAL;  /* FIXME(bnoordhuis) -EBADF */
    
    /* sizeof(socklen_t) != sizeof(int) on some systems. */
    socklen = (socklen_t) *namelen;
    
    if (getpeername(handle->udtfd, name, &socklen))
        return -errno;
    
    *namelen = (int) socklen;
    return 0;
}



int uv_udt_listen(uv_udt_t* udt, int backlog, uv_connection_cb cb) {
    //static int single_accept = -1;
    int err;
    
    if (udt->delayed_error)
        return udt->delayed_error;
    
    /*
    if (single_accept == -1) {
        const char* val = getenv("UV_TCP_SINGLE_ACCEPT");
        single_accept = (val != NULL && atoi(val) != 0);
    }
    
    if (single_accept)
        tcp->flags |= UV_TCP_SINGLE_ACCEPT;
    */
    
    
    /*
    err = maybe_new_socket(udt, AF_INET, UV_STREAM_READABLE);
    if (err)
        return err;
    */
    if (udt_listen(udt->udtfd, backlog))
        return -errno;
    
    udt->connection_cb = cb;
    
    /* Start listening for connections. */
    
    //ev_io_set(&handle->io_watcher, fd, events);
   // uv__io_set_cb(handle, cb);
    
    
    
    udt->io_watcher.cb = uv__server_io;
    uv__io_start(udt->loop, &udt->io_watcher, UV__POLLIN);
    
    return 0;
}


int uv__udt_nodelay(uv_udt_t* handle, int enable) {
    return 0;
}


int uv__udt_keepalive(uv_udt_t* handle, int enable, unsigned int delay) {
    return 0;
}


int uv_udt_nodelay(uv_udt_t* handle, int enable) {
    if (handle->io_watcher.fd != -1 && uv__udt_nodelay(handle, enable))
        return -1;
    
    if (enable)
        handle->flags |= UV_TCP_NODELAY;
    else
        handle->flags &= ~UV_TCP_NODELAY;
    
    return 0;
}



int uv_udt_simultaneous_accepts(uv_udt_t* handle, int enable) {
    return 0;
}


int uv_udt_setrendez(uv_udt_t* handle, int enable) {
    int rndz = enable ? 1 : 0;
    
    if (handle->io_watcher.fd != -1 &&
        udt_setsockopt(handle->udtfd, 0, UDT_UDT_RENDEZVOUS, &rndz, sizeof(rndz)))
        return -1;
    
    if (enable)
        handle->flags |= UV_UDT_RENDEZ;
    else
        handle->flags &= ~UV_UDT_RENDEZ;
    
    return 0;
}

int uv_udt_setqos(uv_udt_t* handle, int qos) {
    if (handle->io_watcher.fd != -1 &&
        udt_setsockopt(handle->udtfd, 0, UDT_UDT_QOS, &qos, sizeof(qos)))
        return -1;
    
    return 0;
}

int uv_udt_setmbw(uv_udt_t* handle, int64_t mbw) {
    if (handle->io_watcher.fd != -1 &&
        udt_setsockopt(handle->udtfd, 0, UDT_UDT_MAXBW, &mbw, sizeof(mbw)))
        return -1;
    
    return 0;
}

int uv_udt_setmbs(uv_udt_t* handle, int32_t mfc, int32_t mudt, int32_t mudp) {
    if (handle->io_watcher.fd != -1 &&
        ((mfc  != -1 ? udt_setsockopt(handle->udtfd, 0, UDT_UDT_FC,     &mfc,  sizeof(mfc))  : 0) ||
         (mudt != -1 ? udt_setsockopt(handle->udtfd, 0, UDT_UDT_SNDBUF, &mudt, sizeof(mudt)) : 0) ||
         (mudt != -1 ? udt_setsockopt(handle->udtfd, 0, UDT_UDT_RCVBUF, &mudt, sizeof(mudt)) : 0) ||
         (mudp != -1 ? udt_setsockopt(handle->udtfd, 0, UDT_UDP_SNDBUF, &mudp, sizeof(mudp)) : 0) ||
         (mudp != -1 ? udt_setsockopt(handle->udtfd, 0, UDT_UDP_RCVBUF, &mudp, sizeof(mudp)) : 0)))
        return -1;
    
    return 0;
}

int uv_udt_setsec(uv_udt_t* handle, int32_t mode, unsigned char key_buf[], int32_t key_len) {
    if (handle->io_watcher.fd != -1 &&
        (udt_setsockopt(handle->udtfd, 0, UDT_UDT_SECKEY, key_buf, (32 < key_len) ? 32 : key_len)) ||
        udt_setsockopt(handle->udtfd, 0, UDT_UDT_SECMOD, &mode, sizeof(mode)))
        return -1;
    
    return 0;
}

int uv_udt_punchhole(uv_udt_t* handle, const struct sockaddr * addr, int32_t from, int32_t to) {
    if (handle->io_watcher.fd != -1 &&
        udt_punchhole(handle->udtfd, addr, sizeof(*addr), from, to))
        return -1;
    
    return 0;
}


int uv_udt_getperf(uv_udt_t* handle, uv_netperf_t* perf, int clear) {
    UDT_TRACEINFO lperf;
    
    memset(&lperf, 0, sizeof(lperf));
    if (handle->io_watcher.fd != -1 &&
        udt_perfmon(handle->udtfd, &lperf, clear))
        return -1;
    
    // transform UDT local performance data
    // notes: it's same
    memcpy(perf, &lperf, sizeof(*perf));
    
    return 0;
}


// UDT socket operation
int udt__socket(int domain, int type, int protocol) {
    int sockfd;
    int optval;
    
    sockfd = udt_socket(domain, type, protocol);
    
    if (sockfd == -1)
        goto out;
    
    // TBD... optimization on mobile device
    /* Set UDT congestion control algorithms */
    if (udt_setccc(sockfd, UDT_CCC_UDT)) {
        udt_close(sockfd);
        sockfd = -1;
    }
    
    /* Set default UDT buffer size */
    // optimization for node.js:
    // - set maxWindowSize from 25600 to 2560, UDT/UDP buffer from 10M/1M to 1M/100K
    // - ??? or            from 25600 to 5120, UDT/UDP buffer from 10M/1M to 2M/200K
    // TBD...
    optval = 5120;
    if (udt_setsockopt(sockfd, 0, (int)UDT_UDT_FC, (void *)&optval, sizeof(optval))) {
        udt_close(sockfd);
        sockfd = -1;
    }
    optval = 204800;
    if (udt_setsockopt(sockfd, 0, (int)UDT_UDP_SNDBUF, (void *)&optval, sizeof(optval)) |
        udt_setsockopt(sockfd, 0, (int)UDT_UDP_RCVBUF, (void *)&optval, sizeof(optval))) {
        udt_close(sockfd);
        sockfd = -1;
    }
    optval = 2048000;
    if (udt_setsockopt(sockfd, 0, (int)UDT_UDT_SNDBUF, (void *)&optval, sizeof(optval)) |
        udt_setsockopt(sockfd, 0, (int)UDT_UDT_RCVBUF, (void *)&optval, sizeof(optval))) {
        udt_close(sockfd);
        sockfd = -1;
    }
    ////////////////////////////////////////////////////////////////////////////////////////
    
    if (udt__nonblock(sockfd, 1)) {
        udt_close(sockfd);
        sockfd = -1;
    }
    
out:
    return sockfd;
}


int udt__accept(int sockfd) {
    int peerfd = -1;
    struct sockaddr_storage saddr;
    int namelen = sizeof saddr;
    
    assert(sockfd >= 0);
    
    if ((peerfd = udt_accept(sockfd, (struct sockaddr *)&saddr, &namelen)) == -1) {
        return -1;
    }
    
    if (udt__nonblock(peerfd, 1)) {
        udt_close(peerfd);
        peerfd = -1;
    }
    
    ///char clienthost[NI_MAXHOST];
    ///char clientservice[NI_MAXSERV];
    
    ///getnameinfo((struct sockaddr*)&saddr, sizeof saddr, clienthost, sizeof(clienthost), clientservice, sizeof(clientservice), NI_NUMERICHOST|NI_NUMERICSERV);
    ///fprintf(stdout, "new connection: %s:%s\n", clienthost, clientservice);
    
    return peerfd;
}


int udt__nonblock(int udtfd, int set)
{
    int block = (set ? 0 : 1);
    int rc1, rc2;
    
    rc1 = udt_setsockopt(udtfd, 0, (int)UDT_UDT_SNDSYN, (void *)&block, sizeof(block));
    rc2 = udt_setsockopt(udtfd, 0, (int)UDT_UDT_RCVSYN, (void *)&block, sizeof(block));
    
    return (rc1 | rc2);
}
