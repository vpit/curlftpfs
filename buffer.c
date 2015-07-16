/*
    FTP file system
    Copyright (C) 2006 Robson Braga Araujo <robsonbraga@gmail.com>

    This program can be distributed under the terms of the GNU GPL.
    See the file COPYING.
*/

#include <stdlib.h> /* realloc(), exit() */
#include <string.h> /* memcpy() */
#include <stdio.h>  /* stderr, fprintf() */

#include <stdint.h> /* <uint8_t> */
#include <unistd.h> /* <off_t> */

#include "buffer.h"

void buf_init(struct buffer *buf) {
  buf->p            = NULL;
  buf->len          = 0;
  buf->size         = 0;
  buf->begin_offset = 0;
}

void buf_free(struct buffer *buf) {
  free(buf->p);
}

void buf_clear(struct buffer *buf) {
  buf_free(buf);
  buf_init(buf);
}

static int buf_resize(struct buffer *buf, size_t len) {
  buf->size = (buf->len + len + 63) & ~31;
  buf->p    = realloc(buf->p, buf->size);

  if (!buf->p) {
    fprintf(stderr, "ftpfs: memory allocation failed\n");
    return -1;
  }

  return 0;
}

int buf_add_mem(struct buffer *buf, const void *data, size_t len) {
  if (buf->len + len > buf->size && buf_resize(buf, len) == -1)
    return -1;

  memcpy(buf->p + buf->len, data, len);
  buf->len += len;

  return 0;
}

void buf_null_terminate(struct buffer *buf) {
  if (buf_add_mem(buf, "\0", 1) == -1)
    exit(1);
}
