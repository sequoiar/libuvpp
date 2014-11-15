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

#include <assert.h>
#include <stdlib.h>
#include <fcntl.h>

#include "uv.h"
#include "internal.h"
#include "handle-inl.h"
#include "stream-inl.h"
#include "req-inl.h"

/* A zero-size buffer for use by uv_device_read */
static char uv_zero_[] = "";

int uv_device_init(uv_loop_t* loop, uv_device_t* handle) {
  memset(handle, 0, sizeof(uv_device_t));

  uv_stream_init(loop, (uv_stream_t*) handle, UV_DEVICE);
  handle->handle = INVALID_HANDLE_VALUE;

  return 0;
};

int uv_device_open(uv_loop_t* loop,
    uv_device_t* handle, const char* path, int flags) {
  DWORD dwCreationDisposition;

  assert(handle && handle->handle == INVALID_HANDLE_VALUE);
  uv_connection_init((uv_stream_t*) handle);

  handle->buf.len = 64 * 1024;
  handle->buf.base = malloc(handle->buf.len);
  if (handle->buf.base == NULL) 
    uv_fatal_error(ERROR_OUTOFMEMORY, "malloc"); 

  dwCreationDisposition = 0;
  if ( flags & O_RDONLY ) 
    dwCreationDisposition |= GENERIC_READ;
  if ( flags & O_WRONLY )
    dwCreationDisposition |= GENERIC_WRITE;
  if ( flags & O_RDWR )
    dwCreationDisposition |= (GENERIC_READ | GENERIC_WRITE);

  handle->handle = CreateFile(path,
    dwCreationDisposition,
    0,
    NULL,
    OPEN_EXISTING,
    FILE_ATTRIBUTE_SYSTEM|FILE_FLAG_OVERLAPPED,
    0);
  if (handle->handle == INVALID_HANDLE_VALUE)
    return GetLastError();

  /* Try to associate with IOCP. */
  if (!CreateIoCompletionPort(handle->handle, loop->iocp, (ULONG_PTR)handle, 0)) {
    DWORD err = GetLastError();
    if (ERROR_INVALID_PARAMETER == err) {
       /* Already associated. */
    } else {
      return uv_translate_sys_error(err);
    }
  }

  handle->flags |= UV_HANDLE_READABLE | UV_HANDLE_WRITABLE;
  return 0;
}

int uv_device_ioctl(uv_device_t* device, int cmd, void* arg) {
  uv_ioargs_t *args = (uv_ioargs_t*) arg;
  BOOL r;

  assert(device && device->handle != INVALID_HANDLE_VALUE);
  assert(args && args->nInBufferSize && args->nOutBufferSize);
  r = DeviceIoControl(device->handle, 
    (DWORD) cmd,
    args->lpInBuffer,
    args->nInBufferSize,
    args->lpOutBuffer,
    args->nOutBufferSize,
    &args->lpBytesReturned,
    NULL);
  if (r) 
    return 0;

  return uv_translate_sys_error(GetLastError());
}

void uv_device_endgame(uv_loop_t* loop, uv_device_t* handle) {
  if (handle->buf.base)
    free(handle->buf.base);

  uv__handle_close(handle);
}

static void uv_device_queue_read(uv_loop_t* loop, uv_device_t* handle) {
  uv_read_t* req;
  BOOL r;
  DWORD err;

  assert(handle->flags & UV_HANDLE_READING);
  assert(!(handle->flags & UV_HANDLE_READ_PENDING));
  assert(handle->handle && handle->handle != INVALID_HANDLE_VALUE);

  req = &handle->read_req;
  memset(&req->overlapped, 0, sizeof(req->overlapped));

  r = ReadFile(handle->handle,
    handle->buf.base,
    handle->buf.len,
    NULL,
    &req->overlapped);
  if (r) {
    handle->flags |= UV_HANDLE_READ_PENDING;
    handle->reqs_pending++;
    uv_insert_pending_req(loop, (uv_req_t*)req);
  } else {
    err = GetLastError();
    if (r == 0 && err == ERROR_IO_PENDING) {
      /* The req will be processed with IOCP. */
      handle->flags |= UV_HANDLE_READ_PENDING;
      handle->reqs_pending++;
    } else {
      /* Make this req pending reporting an error. */
      SET_REQ_ERROR(req, err);
      uv_insert_pending_req(loop, (uv_req_t*)req);
      handle->reqs_pending++;
    }
  } 
};

int uv_device_read_start(uv_device_t* handle, uv_alloc_cb alloc_cb,
    uv_read_cb read_cb) {
  uv_loop_t* loop = handle->loop;

  if (!(handle->flags & UV_HANDLE_READABLE))
    return ERROR_INVALID_PARAMETER;

  handle->flags |= UV_HANDLE_READING;
  INCREASE_ACTIVE_COUNT(loop, handle);
  handle->read_cb = read_cb;
  handle->alloc_cb = alloc_cb;

  /* If reading was stopped and then started again, there could still be a */
  /* read request pending. */
  if (handle->flags & UV_HANDLE_READ_PENDING)
    return 0;

  uv_device_queue_read(loop, handle);
  return 0; 
}


int uv_device_write(uv_loop_t* loop,
                 uv_write_t* req,
                 uv_device_t* handle,
                 const uv_buf_t bufs[],
                 unsigned int nbufs,
                 uv_write_cb cb) {
  int result;
  DWORD err = 0;
  
  if (nbufs != 1 && (nbufs != 0 || !handle)) {
    return ERROR_NOT_SUPPORTED;
  }
  /* Only TCP handles are supported for sharing. */
  if (handle && ((handle->type != UV_DEVICE) ||
    !(handle->flags & UV_HANDLE_CONNECTION))) {
      return ERROR_NOT_SUPPORTED;
  }

  uv_req_init(loop, (uv_req_t*) req);
  req->type = UV_WRITE;
  req->handle = (uv_stream_t*) handle;
  req->cb = cb;
  req->queued_bytes = 0;
  memset(&(req->overlapped), 0, sizeof(req->overlapped));

  result = WriteFile(handle->handle,
    bufs[0].base,
    bufs[0].len,
    NULL,
    &req->overlapped);

  if (result) {
    /* Request completed immediately. */
    req->queued_bytes = 0;
    handle->reqs_pending++;
    handle->write_reqs_pending++;
    REGISTER_HANDLE_REQ(loop, handle, req);
    uv_insert_pending_req(loop, (uv_req_t*) req);
  } else {
    /* Request queued by the kernel. */
    err = GetLastError();
    if (err != ERROR_IO_PENDING)
      return GetLastError();

    req->queued_bytes = uv__count_bufs(bufs, nbufs);
    handle->reqs_pending++;
    handle->write_reqs_pending++;
    REGISTER_HANDLE_REQ(loop, handle, req);
    handle->write_queue_size += req->queued_bytes;
  }
  return 0;
}

void uv_process_device_read_req(uv_loop_t* loop, uv_device_t* handle,
    uv_req_t* req) {
  DWORD err;
  uv_buf_t buf;

  assert(handle->type == UV_DEVICE);

  handle->flags &= ~UV_HANDLE_READ_PENDING;
  assert(handle->flags & UV_HANDLE_READING);

  if (!REQ_SUCCESS(req)) {
    /* An error occurred doing the read. */
    if (handle->flags & UV_HANDLE_READING)  {
      handle->flags &= ~UV_HANDLE_READING;
      DECREASE_ACTIVE_COUNT(loop, handle);
      
      handle->alloc_cb((uv_handle_t*)handle,
        req->overlapped.InternalHigh, &buf);
      memcpy(buf.base,handle->buf.base,req->overlapped.InternalHigh);
      err = GET_REQ_SOCK_ERROR(req);
      handle->read_cb((uv_stream_t*)handle,
                      uv_translate_sys_error(err),
                      &buf);
    }
  }else {
    if (req->overlapped.InternalHigh > 0) {
      /* Successful read */
      handle->alloc_cb((uv_handle_t*)handle, 
        req->overlapped.InternalHigh, &buf);
      memcpy(buf.base,handle->buf.base,req->overlapped.InternalHigh);
      handle->read_cb((uv_stream_t*)handle,
        req->overlapped.InternalHigh,
        &buf);
      /* not sure this, are we need repeat read ? */
      /* Read again only if bytes == buf.len */
      if (req->overlapped.InternalHigh < sizeof(handle->buf))
        goto done;
    } else {
      /* Connection closed */
      if (handle->flags & UV_HANDLE_READING) {
        handle->flags &= ~UV_HANDLE_READING;
        DECREASE_ACTIVE_COUNT(loop, handle);
      }
      handle->flags &= ~UV_HANDLE_READABLE;

      buf.base = 0;
      buf.len = 0;
      handle->read_cb((uv_stream_t*)handle, UV_EOF, &buf);
      goto done;
    }
  }
  /* 
    may be we need more do read operation 
  */
done:
  /* Post another read if still reading and not closing. */
  if ((handle->flags & UV_HANDLE_READING) &&
    !(handle->flags & UV_HANDLE_READ_PENDING)) {
      uv_device_queue_read(loop, handle);
  }

  DECREASE_PENDING_REQ_COUNT(handle);
}


void uv_process_device_write_req(uv_loop_t* loop, uv_device_t* handle,
    uv_write_t* req) {
  int err;

  assert(handle->type == UV_DEVICE);
  assert(handle->write_queue_size >= req->queued_bytes);
  handle->write_queue_size -= req->queued_bytes;

  UNREGISTER_HANDLE_REQ(loop, handle, req);

  if (req->cb) {
    err = GET_REQ_ERROR(req);
    req->cb(req, uv_translate_sys_error(err));
  }

  handle->write_reqs_pending--;

  if (handle->shutdown_req != NULL &&
    handle->write_reqs_pending == 0) {
      uv_want_endgame(loop, (uv_handle_t*)handle);
  }

  DECREASE_PENDING_REQ_COUNT(handle);
}


static int uv_device_try_cancel_io(uv_device_t* device) {
  if (!CancelIo(device->handle)) 
    return GetLastError();

  /* It worked. */
  return 0;
}


void uv_device_close(uv_loop_t* loop, uv_device_t* device) {
  if (device->flags & UV_HANDLE_READING) {
    device->flags &= ~UV_HANDLE_READING;
    DECREASE_ACTIVE_COUNT(loop, device);
  }

  CloseHandle(device->handle);
  device->handle = INVALID_HANDLE_VALUE;

  device->flags &= ~(UV_HANDLE_READABLE | UV_HANDLE_WRITABLE);
  uv__handle_closing(device);

  if (device->reqs_pending == 0)
    uv_want_endgame(device->loop, (uv_handle_t*)device);
}

/* TODO: remove me */
void uv_process_device_accept_req(uv_loop_t* loop, uv_device_t* handle,
  uv_req_t* raw_req) {
    abort();
}

/* TODO: remove me */
void uv_process_device_connect_req(uv_loop_t* loop, uv_device_t* handle,
  uv_connect_t* req) {
    abort();
}
