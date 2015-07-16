#ifndef __CURLFTPFS_ERROR_H__
#define __CURLFTPFS_ERROR_H__ 1

/*
    FTP file system
    Copyright (C) 2015 Vincent Pit <vince@profvince.com>

    This program can be distributed under the terms of the GNU GPL.
    See the file COPYING.
*/

void ftpfs_debug_printf(unsigned int indent, const char *file, int line, const char *fmt, ...);

#define DEBUG(level, ...) \
  do { \
    if (level <= ftpfs.debug) \
      ftpfs_debug_printf(level, __FILE__, __LINE__, __VA_ARGS__); \
  } while (0)

#endif
