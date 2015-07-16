/*
    FTP file system
    Copyright (C) 2015 Vincent Pit <vince@profvince.com>

    This program can be distributed under the terms of the GNU GPL.
    See the file COPYING.
*/

#include <stdarg.h> /* <va_list>, va_*() */
#include <stdio.h>  /* stderr, fprintf(), vfprintf() */
#include <time.h>   /* time() */

#include "error.h"

void ftpfs_debug_printf(unsigned int indent, const char *file, int line, const char *fmt, ...) {
  va_list ap;

  while (indent > 0) {
    fprintf(stderr, " ");
    indent--;
  }

  fprintf(stderr, "%ld %s:%d ", time(NULL), file, line);
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);

  return;
}
