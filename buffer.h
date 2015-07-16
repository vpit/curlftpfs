#ifndef __CURLFTPFS_BUFFER_H__
#define __CURLFTPFS_BUFFER_H__ 1

/*
    FTP file system
    Copyright (C) 2006 Robson Braga Araujo <robsonbraga@gmail.com>

    This program can be distributed under the terms of the GNU GPL.
    See the file COPYING.
*/

struct buffer {
  uint8_t *p;
  size_t   len;
  size_t   size;
  off_t    begin_offset;
};

void buf_init(struct buffer *buf);
void buf_free(struct buffer *buf);
void buf_clear(struct buffer *buf);
int  buf_add_mem(struct buffer *buf, const void *data, size_t len);
void buf_null_terminate(struct buffer *buf);

#endif
