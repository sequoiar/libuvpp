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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <arpa/inet.h> 
#include <sys/select.h>
#include <sys/time.h>
#include <errno.h>
#include <stdarg.h> 
#include <assert.h>

#define ASSERT assert

#ifdef WIN32
#error This only works on windows
#endif

typedef struct {
  uv_write_t req;
  uv_buf_t buf;
} write_req_t;

static uv_loop_t* loop;


static void after_write(uv_write_t* req, int status);
static void after_read(uv_stream_t*, ssize_t nread, const uv_buf_t* buf);
static void on_close(uv_handle_t* peer);

static int step = 0;
static void after_write(uv_write_t* req, int status) {
  write_req_t* wr;

  /* Free the read/write buffer and the request */
  wr = (write_req_t*) req;
  free(wr->buf.base);
  free(wr);

  if (step > 10) {
    uv_close((uv_handle_t*)req->handle,on_close);
  }
  step += 1;

  if (status == 0)
    return;

  fprintf(stderr,
          "uv_write error: %s - %s\n",
          uv_err_name(status),
          uv_strerror(status));
}


static void after_shutdown(uv_shutdown_t* req, int status) {
  uv_close((uv_handle_t*) req->handle, on_close);
  free(req);
}


static void after_read(uv_stream_t* handle,
                       ssize_t nread,
                       const uv_buf_t* buf) {
  int i;
  write_req_t *wr;
  uv_shutdown_t* sreq;

  if (nread < 0) {
    /* Error or EOF */
    ASSERT(nread == UV_EOF);

    free(buf->base);
    sreq = malloc(sizeof* sreq);
    ASSERT(0 == uv_shutdown(sreq, handle, after_shutdown));
    return;
  }

  if (nread == 0) {
    /* Everything OK, but nothing read. */
    free(buf->base);
    return;
  }

  /*
   * Scan for the letter Q which signals that we should quit the server.
   * If we get QS it means close the stream.
   */
  ASSERT(nread>20);
  if (nread>20 && buf->len > 20) {
    uint8_t ip[4];
    memcpy(ip,buf->base+12,4);
    memcpy(buf->base+12,buf->base+16,4);
    memcpy(buf->base+16,ip,4);
  } else {
    printf("data %p len:%d\n", buf->base,buf->len);
  }

  wr = (write_req_t*) malloc(sizeof *wr);
  ASSERT(wr != NULL);
  wr->buf = uv_buf_init(buf->base, nread);

  if (uv_write(&wr->req, handle, &wr->buf, 1, after_write)) {
    printf("uv_write failed\n");
    abort();
  }
}


static void on_close(uv_handle_t* peer) {
  printf("close %p\n", (void*)peer);
}


static void echo_alloc(uv_handle_t* handle,
                       size_t suggested_size,
                       uv_buf_t* buf) {
  buf->base = malloc(suggested_size);
  buf->len = suggested_size;
}

int main() {
  loop = uv_default_loop();
  uv_device_t device;
  int r;
  struct ifreq ifr;
  char dev[IFNAMSIZ] = "";
  int flags = IFF_TUN|IFF_NO_PI;

  r = uv_device_open(loop, &device, "/dev/net/tun", O_RDWR);
  if (r) {
    fprintf(stderr, "uv_pipe_init: %s\n", uv_strerror(r));
    return 1;
  }

  memset(&ifr, 0, sizeof(ifr));

  strcpy(dev,"tun0");
  ifr.ifr_flags = flags;

  if (*dev) {
    strncpy(ifr.ifr_name, dev, IFNAMSIZ);
  }

  if( (r = uv_device_ioctl(&device, TUNSETIFF, (void *)&ifr)) < 0 ) {
    fprintf(stderr, "uv_device_ioctl: %s\n", uv_strerror(r));
    uv_close((uv_handle_t*)&device,on_close);
    return r;
  }

  strcpy(dev, ifr.ifr_name); 
  r = fork();
  if(r==0)
  {
    system("ifconfig tun0 10.3.0.1 netmask 255.255.255.253 pointopoint 10.3.0.2");
    system("ping 10.3.0.2 -c 10");
    exit(0);
  } else {
    printf("SETUP %s\n", dev);

    r = uv_read_start((uv_stream_t*)&device, echo_alloc, after_read);
    if (r) {
      fprintf(stderr, "uv_read_start: %s\n", uv_strerror(r));
      return 1;
    }

    uv_run(loop, UV_RUN_DEFAULT);
    return 0;
  }
}
