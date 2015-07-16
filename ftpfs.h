#ifndef __CURLFTPFS_FTPFS_H__
#define __CURLFTPFS_FTPFS_H__

/*
    FTP file system
    Copyright (C) 2006 Robson Braga Araujo <robsonbraga@gmail.com>

    This program can be distributed under the terms of the GNU GPL.
    See the file COPYING.
*/

#include "config.h"

#ifdef HAVE_LINUX_LIMITS_H
#  include <linux/limits.h>
#else
#  include <limits.h>
#endif
#ifndef PATH_MAX
#  define PATH_MAX 1024
#endif

#include <fuse.h>
#include <curl/curl.h>
#include <curl/easy.h>
#include <pthread.h> /* <pthread_mutex_t> */

struct ftpfs {
  char* host;
  char* mountpoint;
  pthread_mutex_t lock;
  CURL* connection;
  CURLM* multi;
  int attached_to_multi;
  struct ftpfs_file* current_fh;
  unsigned blksize;
  int verbose;
  int debug;
  int transform_symlinks;
  int disable_epsv;
  int skip_pasv_ip;
  char* ftp_method;
  char* custom_list;
  int tcp_nodelay;
  char* ftp_port;
  int disable_eprt;
  int connect_timeout;
  int use_ssl;
  int no_verify_hostname;
  int no_verify_peer;
  char* cert;
  char* cert_type;
  char* key;
  char* key_type;
  char* key_password;
  char* engine;
  char* cacert;
  char* capath;
  char* ciphers;
  char* interface;
  char* krb4;
  char* proxy;
  int proxytunnel;
  int proxyanyauth;
  int proxybasic;
  int proxydigest;
  int proxyntlm;
  int proxytype;
  char* user;
  char* proxy_user;
  int ssl_version;
  int ip_version;
  char symlink_prefix[PATH_MAX+1];
  size_t symlink_prefix_len;
  curl_version_info_data* curl_version;
  int safe_nobody;
  int tryutf8;
  const char *codepage;
  const char *iocharset;
  int multiconn;
};

extern struct ftpfs ftpfs;

extern struct fuse_cache_operations ftpfs_oper;

#define CURLFTPFS_BAD_NOBODY 0x070f02
#define CURLFTPFS_BAD_SSL    0x070f03

#define CURLFTPFS_BAD_READ   ((size_t)-1)

void cancel_previous_multi(void);
void set_common_curl_stuff(CURL* easy);

void ftpfs_curl_easy_setopt_abort(void);

#define curl_easy_setopt_or_die(handle, option, ...) \
  do { \
    if (curl_easy_setopt(handle, option, __VA_ARGS__) != CURLE_OK) \
      ftpfs_curl_easy_setopt_abort(); \
  } while (0)

void ftpfs_curl_easy_perform_abort(void);

#endif   /* __CURLFTPFS_FTPFS_H__ */
