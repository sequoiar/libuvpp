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

#include <stdlib.h>
#include <stdio.h>
#include <string.h> /* strlen */


#define SERVER_MAX_NUM 10
#define CLIENT_MAX_NUM 10

/* Run the benchmark for this many ms */
#define TIME 2000

typedef struct {
  int pongs;
  int state;
  uv_udt_t udt;
  uv_connect_t connect_req;
  uv_shutdown_t shutdown_req;
} pinger_t;

typedef struct buf_s {
  uv_buf_t uv_buf_t;
  struct buf_s* next;
} buf_t;


static char PING[] = "PING\n";

static uv_loop_t* loop;

static buf_t* buf_freelist = NULL;
static int pinger_shutdown_cb_called;
static int completed_pingers = 0;
static int64_t start_time;



static void buf_alloc(uv_handle_t* udt, size_t size, uv_buf_t* buf) {

    buf->base = malloc(size);
    buf->len = size;
}


static void buf_free(uv_buf_t* buf) {
    free(buf->base);
}


static void pinger_close_cb(uv_handle_t* handle) {
  pinger_t* pinger;

  pinger = (pinger_t*)handle->data;
  //LOGF("ping_pongs: %d roundtrips/s\n", (1000 * pinger->pongs) / TIME);

  free(pinger);

  completed_pingers++;
}


static void pinger_write_cb(uv_write_t* req, int status) {
  ASSERT(status == 0);
    //printf("pinger_write_cb");
  free(req);
}


static void pinger_write_ping(pinger_t* pinger) {
  uv_write_t* req;
  uv_buf_t buf;

  buf.base = (char*)&PING;
  buf.len = strlen(PING);

  req = malloc(sizeof *req);
  if (uv_write(req, (uv_stream_t*) &pinger->udt, &buf, 1, pinger_write_cb)) {
    FATAL("uv_write failed");
  }
}


static void pinger_shutdown_cb(uv_shutdown_t* req, int status) {
  ///printf("pinger_shutdown_cb\n");

  ASSERT(status == 0);
  pinger_shutdown_cb_called++;


  ///ASSERT(completed_pingers == 0);
}


static void pinger_read_cb(uv_stream_t* udt, ssize_t nread, const uv_buf_t* buf) {
  ssize_t i;
  pinger_t* pinger;

  //printf("%s.%d,pinger_read_cb,udtfd@%d\n", __FUNCTION__, __LINE__, ((uv_udt_t*)udt)->udtfd);

  pinger = (pinger_t*)udt->data;

  if (nread < 0) {
    ///printf("%s.%d,pinger_read_cb,udtfd@%d\n", __FUNCTION__, __LINE__, ((uv_udt_t*)udt)->udtfd);
    ///ASSERT(uv_last_error(loop).code == UV_EOF);

    if (buf->base) {
      buf_free(buf);
    }

    ///ASSERT(pinger_shutdown_cb_called == 1);
    uv_close((uv_handle_t*)udt, pinger_close_cb);

    return;
  }


  for (i = 0; i < nread; i++) {
    ASSERT(buf->base[i] == PING[pinger->state]);
    pinger->state = (pinger->state + 1) % (sizeof(PING) - 1);
    if (pinger->state == 0) {
      pinger->pongs++;
      if (uv_now(loop) - start_time > TIME) {
        uv_shutdown(&pinger->shutdown_req, (uv_stream_t*) udt, pinger_shutdown_cb);
        break;
      } else {
        pinger_write_ping(pinger);
      }
    }
  }

  buf_free(buf);
}


static void pinger_connect_cb(uv_connect_t* req, int status) {
  pinger_t *pinger = (pinger_t*)req->handle->data;

  //printf("pinger_connect_cb\n");
  ASSERT(status == 0);

  pinger_write_ping(pinger);

  if (uv_read_start(req->handle, buf_alloc, pinger_read_cb)) {
    FATAL("uv_read_start failed");
  }
}

 

static void pinger_new(int port) {
  int r;
    struct sockaddr client_addr;
    uv_ip4_addr("0.0.0.0", 0, &client_addr);
    struct sockaddr_in server_addr;
    uv_ip4_addr("127.0.0.1", port, &server_addr);
    
    
    
  pinger_t *pinger;

  pinger = (pinger_t*)malloc(sizeof(*pinger));
  pinger->state = 0;
  pinger->pongs = 0;

    
  
  r = uv_udt_init(loop, &pinger->udt);
    if(r != 0){
        fprintf(stderr, "uv_udt_init %s\n", uv_strerror(r));
    }
  ASSERT(!r);

  pinger->udt.data = pinger;

  uv_udt_bind(&pinger->udt, &client_addr, 0);

  r = uv_udt_connect(&pinger->connect_req, &pinger->udt, &server_addr, pinger_connect_cb);
    if(r != 0){
        fprintf(stderr, "uv_udt_connect %s\n", uv_strerror(r));
    }
  ASSERT(!r);
    
}


int main(int argc, char * argv [])
{
    printf("echo-client-udt start \n");
    
    int srvNum=1;
	int clnNum=1;
	int port = TEST_PORT;
	int i=0;
	int j=0;
	loop = uv_default_loop();

	start_time = uv_now(loop);

	if (argc == 4) {
	    port    = atoi(argv[1]);
	    srvNum  = atoi(argv[2]);
		clnNum  = atoi(argv[3]);
	} else if (argc == 3) {
	    port   = atoi(argv[1]);
	    srvNum = atoi(argv[2]);
	} else if (argc == 2) {
	    port = atoi(argv[1]);
	}
	srvNum = (srvNum < SERVER_MAX_NUM)? srvNum : SERVER_MAX_NUM;
	clnNum = (clnNum < CLIENT_MAX_NUM)? clnNum : CLIENT_MAX_NUM;
	
	for (i = 0; i < srvNum; i++)
		for (j = 0; j < clnNum; j++)
		    pinger_new(port+i);

	uv_run(loop, UV_RUN_DEFAULT);

	//ASSERT(completed_pingers == 1);

	return 0;
}
