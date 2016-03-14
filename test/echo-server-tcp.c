/* Copyright Joyent, Inc. and other Node contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "uv.h"
#include "task.h"
#include <stdio.h>
#include <stdlib.h>


#define SERVER_MAX_NUM 1

typedef struct {
  uv_write_t req;
  uv_buf_t buf;
} write_req_t;

static uv_loop_t* loop;

static stream_type serverType;
static uv_tcp_t tcpServer[SERVER_MAX_NUM];

static void after_write(uv_write_t* req, int status);
static void after_read(uv_stream_t*, ssize_t nread, uv_buf_t* buf);
static void on_close(uv_handle_t* peer);
static void on_server_close(uv_handle_t* handle);
static void on_connection(uv_stream_t*, int status);


static void after_write(uv_write_t* req, int status) {
  write_req_t* wr;
  ///printf("%s.%d\n", __FUNCTION__, __LINE__);
    if (status < 0) {
        fprintf(stderr, "uv_write error: %s\n", uv_strerror(status));
        // error!
        return;
    }
    

  wr = (write_req_t*) req;

  /* Free the read/write buffer and the request */
  free(wr->buf.base);
  free(wr);
}


static void after_shutdown(uv_shutdown_t* req, int status) {
  ///printf("%s.%d\n", __FUNCTION__, __LINE__);
  uv_close((uv_handle_t*)req->handle, on_close);
  free(req);
}


static void after_read(uv_stream_t* handle, ssize_t nread, uv_buf_t* buf) {
  int i;
  write_req_t *wr;
  uv_shutdown_t* req;

  printf("%s.%d\n", __FUNCTION__, __LINE__);
    if (nread < 0) {
        fprintf(stderr, "Read error %s\n", uv_err_name(nread));
        uv_close((uv_handle_t*) req, NULL);
        free(buf->base);
        return;
    }

  if (nread == 0) {
    /* Everything OK, but nothing read. */
    free(buf->base);
    return;
  }

  ///fprintf(stdout, "echo server recv: %dbytes\n", nread);
  ///fprintf(stdout, ".");
  ///free(buf.base);
  ///return;

  /*
   * Scan for the letter Q which signals that we should quit the server.
   * If we get QS it means close the stream.
   */
    for (i = 0; i < nread; i++) {
      if (buf->base[i] == 'Q') {
        if (i + 1 < nread && buf->base[i + 1] == 'S') {
          free(buf->base);
          uv_close((uv_handle_t*)handle, on_close);
          return;
        } else {
          uv_close((uv_handle_t*)(((uv_handle_t*)handle)->data), on_server_close);
        }
      }
    }

  wr = (write_req_t*) malloc(sizeof *wr);

  wr->buf = uv_buf_init(buf->base, nread);
  if (uv_write(&wr->req, handle, &wr->buf, 1, after_write)) {
    FATAL("uv_write failed");
  }
}


static void on_close(uv_handle_t* peer) {
  ///printf("%s.%d\n", __FUNCTION__, __LINE__);
  free(peer);
}



static void echo_alloc(uv_handle_t* tcp, size_t size, uv_buf_t* buf) {
    
    buf->base = malloc(size);
    buf->len = size;
}

static void on_connection(uv_stream_t* server, int status) {
  uv_stream_t* stream;
  int r;
    printf("on_connection\n");

    if (status < 0) {
        fprintf(stderr, "Connect error %s\n", uv_strerror(status));
        // error!
        return;
    }
    
    
  ASSERT(status == 0);

  switch (serverType) {

  case TCP:
      stream = malloc(sizeof(uv_tcp_t));
      ASSERT(stream != NULL);
      r = uv_tcp_init(loop, (uv_tcp_t*)stream);
      ASSERT(r == 0);
      break;

  default:
    ASSERT(0 && "Bad serverType");
    abort();
  }

  /* associate server with stream */
  stream->data = server;

  r = uv_accept(server, stream);
  ASSERT(r == 0);
  if (r) {
	  free(stream);
	  return;
  }

  r = uv_read_start(stream, echo_alloc, after_read);
  ASSERT(r == 0);
  if (r) {
	  free(stream);
	  return;
  }
}


static void on_server_close(uv_handle_t* handle) {
  ///printf("going on %s:%d\n", __FUNCTION__, __LINE__);
  ///ASSERT(handle == server);
}


static int tcp4_echo_start(int port) {
	struct sockaddr_in addr;
	int r;
	int i;

	serverType = TCP;
	for (i = 0; i < (sizeof(tcpServer) / sizeof(tcpServer[0])); i ++) {
		 uv_ip4_addr("0.0.0.0", port+i, &addr);

		r = uv_tcp_init(loop, &tcpServer[i]);
		if (r < 0) {
			/* TODO: Error codes */
			fprintf(stderr, "Socket creation error\n");
			return 1;
		}

		r = uv_tcp_bind(&tcpServer[i], &addr, 0);
		if (r < 0) {
			/* TODO: Error codes */
			fprintf(stderr, "Bind error\n");
			return 1;
		}
        
		r = uv_listen((uv_stream_t*)&tcpServer[i], SOMAXCONN, on_connection);

        if (r < 0) {
            fprintf(stderr, "Listen error %s\n", uv_strerror(r));
            return 1;
        }
        
	}

	return 0;
}

int main(int argc, char * argv [])
{
    
    printf("echo-server-tcp start\n");
	loop = uv_default_loop();

	if (argc == 2) {
		if (tcp4_echo_start(atoi(argv[1])))
			return 1;
	} else {
		if (tcp4_echo_start(TEST_PORT))
			return 1;
	}

	uv_run(loop, UV_RUN_ONCE);
	return 0;
}
