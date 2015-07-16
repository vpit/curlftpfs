/*
    FTP file system
    Copyright (C) 2015 Vincent Pit <vince@profvince.com>

    This program can be distributed under the terms of the GNU GPL.
    See the file COPYING.
*/

#if !defined(__STDC_VERSION__) || (__STDC_VERSION__ < 199901L)
#  undef  _BSD_SOURCE
#  define _BSD_SOURCE /* For snprintf() */
#endif

#include "config.h"

#undef HAVE_READPASSPHRASE

#if !defined(HAVE_READPASSPHRASE) && defined(HAVE_GETPASS)
#  undef  _BSD_SOURCE
#  define _BSD_SOURCE /* For getpass() */
#endif

#include <stddef.h> /* <size_t> */
#include <stdlib.h> /* realloc() */
#include <string.h> /* memset(), memcpy(), strlen(), strchr() */
#include <stdio.h>  /* snprintf() */
#include <assert.h> /* assert() */

#include "passwd.h"

#ifdef HAVE_READPASSPHRASE
#  include <readpassphrase.h> /* readpassphrase(), RPP_* */
#elif defined(HAVE_GETPASS) && defined(HAVE_UNISTD_H)
#  include <unistd.h> /* getpass() */
#  if !defined(PASS_MAX) && defined(HAVE_PWD_H)
#    include <pwd.h> /* On Mac OS X, we may use _PASSWORD_LEN */
#    ifdef _PASSWORD_LEN
#      define PASS_MAX _PASSWORD_LEN
#    endif
#  endif
#  ifndef PASS_MAX
#    include <limits.h> /* May be in there if PASS_MAX <= 8 */
#  endif
#else
#  error "Need readpassphrase() or getpass()"
#endif

#ifdef HAVE_READPASSPHRASE
#  define FTPFS_PASS_MAX 128
#elif defined(PASS_MAX)
#  define FTPFS_PASS_MAX PASS_MAX
#else
#  define FTPFS_PASS_MAX 8 /* Safe default */
#endif
#define FTPFS_PASS_ERR (FTPFS_PASS_MAX + 1)

static size_t read_passwd(const char *prompt, char *pwd, size_t pwd_size) {
  size_t len;

#ifdef HAVE_READPASSPHRASE
  if (!readpassphrase(prompt, pwd, pwd_size, RPP_ECHO_OFF))
    return FTPFS_PASS_ERR;
  len = strlen(pwd);
#else
  char *tmp = getpass(prompt);
  len = strlen(tmp);
  if (len + 1 > pwd_size) {
    len = FTPFS_PASS_ERR;
  } else {
    memcpy(pwd, tmp, len);
    pwd[len] = 0;
  }
#ifdef PASS_MAX
  /* Clear the internal buffer if we know its size */
  memset(tmp, 0, PASS_MAX);
#endif
#endif

  return len;
}

int prompt_passwd(const char *what, char **userpwd) {
  char   prompt[256];
  char   passwd_buf[FTPFS_PASS_MAX + 1];
  char  *ptr;
  size_t user_len, passwd_len;

  assert(*userpwd);

  /* Already have a password part? */
  if (strchr(*userpwd, ':'))
    return 1;

  snprintf(prompt, sizeof prompt,"Enter %s password for user '%s':",
                                        what,                 *userpwd);

  passwd_len = read_passwd(prompt, passwd_buf, sizeof passwd_buf);
  if (passwd_len == FTPFS_PASS_ERR)
    return 0;

  /* Extend the allocated memory area to fit the password too */
  user_len = strlen(*userpwd);
  ptr      = realloc(*userpwd, user_len + 1 + passwd_len + 1);
  if (!ptr)
    return 0;

  /* Append the password separated with a colon */
  ptr[user_len] = ':';
  memcpy(ptr + user_len + 1, passwd_buf, passwd_len);
  ptr[user_len + 1 + passwd_len] = 0;
  *userpwd = ptr;

  memset(passwd_buf, 0, sizeof passwd_buf);

  return 1;
}
