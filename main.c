/*
    FTP file system
    Copyright (C) 2006 Robson Braga Araujo <robsonbraga@gmail.com>

    This program can be distributed under the terms of the GNU GPL.
    See the file COPYING.
*/

#include <stddef.h> /* offsetof() */
#include <stdlib.h> /* exit() */
#include <string.h> /* memset() */
#include <stdio.h>  /* fprintf(), stderr */

#include <pthread.h> /* pthread_*() */

#include <curl/curl.h>
#include <curl/easy.h>
#include <fuse.h>
#include <glib.h>

#include "ftpfs.h"         /* ftpfs */
#include "cache.h"         /* cache_init(), CACHE_* */
#include "charset_utils.h" /* convert_charsets() */
#include "passwd.h"        /* prompt_passwd() */

#include "config.h" /* VERSION */

enum {
  KEY_HELP,
  KEY_VERBOSE,
  KEY_VERSION
};

#define FTPFS_OPT(t, p, v) { t, offsetof(struct ftpfs, p), v }

static struct fuse_opt ftpfs_opts[] = {
  FTPFS_OPT("ftpfs_debug=%u",     debug, 0),
  FTPFS_OPT("transform_symlinks", transform_symlinks, 1),
  FTPFS_OPT("disable_epsv",       disable_epsv, 1),
  FTPFS_OPT("enable_epsv",        disable_epsv, 0),
  FTPFS_OPT("skip_pasv_ip",       skip_pasv_ip, 1),
  FTPFS_OPT("ftp_port=%s",        ftp_port, 0),
  FTPFS_OPT("disable_eprt",       disable_eprt, 1),
  FTPFS_OPT("ftp_method=%s",      ftp_method, 0),
  FTPFS_OPT("custom_list=%s",     custom_list, 0),
  FTPFS_OPT("tcp_nodelay",        tcp_nodelay, 1),
  FTPFS_OPT("connect_timeout=%u", connect_timeout, 0),
  FTPFS_OPT("ssl",                use_ssl, CURLFTPSSL_ALL),
  FTPFS_OPT("ssl_control",        use_ssl, CURLFTPSSL_CONTROL),
  FTPFS_OPT("ssl_try",            use_ssl, CURLFTPSSL_TRY),
  FTPFS_OPT("no_verify_hostname", no_verify_hostname, 1),
  FTPFS_OPT("no_verify_peer",     no_verify_peer, 1),
  FTPFS_OPT("cert=%s",            cert, 0),
  FTPFS_OPT("cert_type=%s",       cert_type, 0),
  FTPFS_OPT("key=%s",             key, 0),
  FTPFS_OPT("key_type=%s",        key_type, 0),
  FTPFS_OPT("pass=%s",            key_password, 0),
  FTPFS_OPT("engine=%s",          engine, 0),
  FTPFS_OPT("cacert=%s",          cacert, 0),
  FTPFS_OPT("capath=%s",          capath, 0),
  FTPFS_OPT("ciphers=%s",         ciphers, 0),
  FTPFS_OPT("interface=%s",       interface, 0),
  FTPFS_OPT("krb4=%s",            krb4, 0),
  FTPFS_OPT("proxy=%s",           proxy, 0),
  FTPFS_OPT("proxytunnel",        proxytunnel, 1),
  FTPFS_OPT("proxy_anyauth",      proxyanyauth, 1),
  FTPFS_OPT("proxy_basic",        proxybasic, 1),
  FTPFS_OPT("proxy_digest",       proxydigest, 1),
  FTPFS_OPT("proxy_ntlm",         proxyntlm, 1),
  FTPFS_OPT("httpproxy",          proxytype, CURLPROXY_HTTP),
  FTPFS_OPT("socks4",             proxytype, CURLPROXY_SOCKS4),
  FTPFS_OPT("socks5",             proxytype, CURLPROXY_SOCKS5),
  FTPFS_OPT("user=%s",            user, 0),
  FTPFS_OPT("proxy_user=%s",      proxy_user, 0),
  FTPFS_OPT("tlsv1",              ssl_version, CURL_SSLVERSION_TLSv1),
  FTPFS_OPT("sslv3",              ssl_version, CURL_SSLVERSION_SSLv3),
  FTPFS_OPT("ipv4",               ip_version, CURL_IPRESOLVE_V4),
  FTPFS_OPT("ipv6",               ip_version, CURL_IPRESOLVE_V6),
  FTPFS_OPT("utf8",               tryutf8, 1),
  FTPFS_OPT("codepage=%s",        codepage, 0),
  FTPFS_OPT("iocharset=%s",       iocharset, 0),
  FTPFS_OPT("nomulticonn",        multiconn, 0),

  FUSE_OPT_KEY("-h",             KEY_HELP),
  FUSE_OPT_KEY("--help",         KEY_HELP),
  FUSE_OPT_KEY("-v",             KEY_VERBOSE),
  FUSE_OPT_KEY("--verbose",      KEY_VERBOSE),
  FUSE_OPT_KEY("-V",             KEY_VERSION),
  FUSE_OPT_KEY("--version",      KEY_VERSION),
  FUSE_OPT_END
};

static void ftpfs_usage(const char *progname) {
  fprintf(stderr,
"usage: %s <ftphost> <mountpoint>\n"
"\n"
"CurlFtpFS options:\n"
"    -o opt,[opt...]        ftp options\n"
"    -v   --verbose         make libcurl print verbose debug\n"
"    -h   --help            print help\n"
"    -V   --version         print version\n"
"\n"
"FTP options:\n"
"    ftpfs_debug         print some debugging information\n"
"    transform_symlinks  prepend mountpoint to absolute symlink targets\n"
"    disable_epsv        use PASV, without trying EPSV first (default)\n"
"    enable_epsv         try EPSV before reverting to PASV\n"
"    skip_pasv_ip        skip the IP address for PASV\n"
"    ftp_port=STR        use PORT with address instead of PASV\n"
"    disable_eprt        use PORT, without trying EPRT first\n"
"    ftp_method          [multicwd/singlecwd] Control CWD usage\n"
"    custom_list=STR     Command used to list files. Defaults to \"LIST -a\"\n"
"    tcp_nodelay         use the TCP_NODELAY option\n"
"    connect_timeout=N   maximum time allowed for connection in seconds\n"
"    ssl                 enable SSL/TLS for both control and data connections\n"
"    ssl_control         enable SSL/TLS only for control connection\n"
"    ssl_try             try SSL/TLS first but connect anyway\n"
"    no_verify_hostname  does not verify the hostname (SSL)\n"
"    no_verify_peer      does not verify the peer (SSL)\n"
"    cert=STR            client certificate file (SSL)\n"
"    cert_type=STR       certificate file type (DER/PEM/ENG) (SSL)\n"
"    key=STR             private key file name (SSL)\n"
"    key_type=STR        private key file type (DER/PEM/ENG) (SSL)\n"
"    pass=STR            pass phrase for the private key (SSL)\n"
"    engine=STR          crypto engine to use (SSL)\n"
"    cacert=STR          file with CA certificates to verify the peer (SSL)\n"
"    capath=STR          CA directory to verify peer against (SSL)\n"
"    ciphers=STR         SSL ciphers to use (SSL)\n"
"    interface=STR       specify network interface/address to use\n"
"    krb4=STR            enable krb4 with specified security level\n"
"    proxy=STR           use host:port HTTP proxy\n"
"    proxytunnel         operate through a HTTP proxy tunnel (using CONNECT)\n"
"    proxy_anyauth       pick \"any\" proxy authentication method\n"
"    proxy_basic         use Basic authentication on the proxy\n"
"    proxy_digest        use Digest authentication on the proxy\n"
"    proxy_ntlm          use NTLM authentication on the proxy\n"
"    httpproxy           use a HTTP proxy (default)\n"
"    socks4              use a SOCKS4 proxy\n"
"    socks5              use a SOCKS5 proxy\n"
"    user=STR            set server user and password\n"
"    proxy_user=STR      set proxy user and password\n"
"    tlsv1               use TLSv1 (SSL)\n"
"    sslv3               use SSLv3 (SSL)\n"
"    ipv4                resolve name to IPv4 address\n"
"    ipv6                resolve name to IPv6 address\n"
"    utf8                try to transfer file list with utf-8 encoding\n"
"    codepage=STR        set the codepage the server uses\n"
"    iocharset=STR       set the charset used by the client\n"
"\n"
"CurlFtpFS cache options:  \n"
"    cache=yes|no              enable/disable cache (default: yes)\n"
"    cache_timeout=SECS        set timeout for stat, dir, link at once\n"
"                              default is %d seconds\n"
"    cache_stat_timeout=SECS   set stat timeout\n"
"    cache_dir_timeout=SECS    set dir timeout\n"
"    cache_link_timeout=SECS   set link timeout\n"
"\n", progname, DEFAULT_CACHE_TIMEOUT);
}

static int ftpfs_fuse_main(struct fuse_args *args) {
#if FUSE_VERSION >= 26
  return fuse_main(args->argc, args->argv, cache_init(&ftpfs_oper), NULL);
#else
  return fuse_main(args->argc, args->argv, cache_init(&ftpfs_oper));
#endif
}

static int ftpfs_opt_proc(void *data, const char *arg, int key,
                          struct fuse_args *outargs) {
  (void) data;
  (void) outargs;

  switch (key) {
    case FUSE_OPT_KEY_OPT:
      return 1;
    case FUSE_OPT_KEY_NONOPT:
      if (!ftpfs.host) {
        const char* prefix = "";
        if (strncmp(arg, "ftp://", 6) && strncmp(arg, "ftps://", 7)) {
          prefix = "ftp://";
        }
        ftpfs.host = g_strdup_printf("%s%s%s", prefix, arg,
                                     arg[strlen(arg)-1] == '/' ? "" : "/");
        return 0;
      } else if (!ftpfs.mountpoint) {
        ftpfs.mountpoint = strdup(arg);
      }
      return 1;
    case KEY_HELP:
      ftpfs_usage(outargs->argv[0]);
      fuse_opt_add_arg(outargs, "-ho");
      ftpfs_fuse_main(outargs);
      exit(1);
    case KEY_VERBOSE:
      ftpfs.verbose = 1;
      return 0;
    case KEY_VERSION:
      fprintf(stderr, "curlftpfs %s libcurl/%s fuse/%u.%u\n",
              VERSION,
              ftpfs.curl_version->version,
              FUSE_MAJOR_VERSION,
              FUSE_MINOR_VERSION);
      exit(1);
    default:
      exit(1);
  }
}

#if FUSE_VERSION == 25

static int fuse_opt_insert_arg(struct fuse_args *args, int pos, const char *arg){
  assert(pos <= args->argc);
  if (fuse_opt_add_arg(args, arg) == -1)
    return -1;

  if (pos != args->argc - 1) {
    char *newarg = args->argv[args->argc - 1];
    memmove(args->argv + pos + 1, args->argv + pos,
            sizeof(char *) * (args->argc - pos - 1));
    args->argv[pos] = newarg;
  }

  return 0;
}

#endif

int main(int argc, char** argv) {
  CURL            *easy;
  CURLcode         curl_res;
  char            *tmp;
  int              res;
  struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

  /* Initialize curl library before we are a multithreaded program */
  curl_global_init(CURL_GLOBAL_ALL);

  memset(&ftpfs, 0, sizeof ftpfs);

  /* Set some default values */
  ftpfs.curl_version = curl_version_info(CURLVERSION_NOW);
  ftpfs.safe_nobody  = ftpfs.curl_version->version_num > CURLFTPFS_BAD_NOBODY;
  ftpfs.blksize      = 4096;
  ftpfs.disable_epsv = 1;
  ftpfs.multiconn    = 1;
  ftpfs.attached_to_multi = 0;

  if (fuse_opt_parse(&args, &ftpfs, ftpfs_opts, ftpfs_opt_proc) == -1)
    return 1;

  if (!ftpfs.host) {
    fprintf(stderr, "missing host\n");
    fprintf(stderr, "see `%s -h' for usage\n", argv[0]);
    return 1;
  }

  if (!ftpfs.iocharset) {
    ftpfs.iocharset = "UTF8";
  }

  if (ftpfs.codepage) {
    convert_charsets(ftpfs.iocharset, ftpfs.codepage, &ftpfs.host);
  }

  easy = curl_easy_init();
  if (easy == NULL) {
    fprintf(stderr, "Error initializing libcurl\n");
    return 1;
  }

  res = cache_parse_options(&args);
  if (res == -1)
    return 1;

  if (!prompt_passwd("host",  &ftpfs.user))
    return 1;

  if (!prompt_passwd("proxy", &ftpfs.proxy_user))
    return 1;

  if (ftpfs.transform_symlinks && !ftpfs.mountpoint) {
    fprintf(stderr, "cannot transform symlinks: no mountpoint given\n");
    return 1;
  }
  if (!ftpfs.transform_symlinks)
    ftpfs.symlink_prefix_len = 0;
  else if (realpath(ftpfs.mountpoint, ftpfs.symlink_prefix) != NULL)
    ftpfs.symlink_prefix_len = strlen(ftpfs.symlink_prefix);
  else {
    fprintf(stderr, "unable to normalize mount path\n");
    return 1;
  }

  set_common_curl_stuff(easy);
  curl_easy_setopt_or_die(easy, CURLOPT_WRITEDATA, NULL);
  curl_easy_setopt_or_die(easy, CURLOPT_NOBODY, ftpfs.safe_nobody);
  curl_res = curl_easy_perform(easy);
  if (curl_res != 0)
    ftpfs_curl_easy_perform_abort();
  curl_easy_setopt_or_die(easy, CURLOPT_NOBODY, 0);

  ftpfs.multi = curl_multi_init();
  if (ftpfs.multi == NULL) {
    fprintf(stderr, "Error initializing libcurl multi\n");
    return 1;
  }

  ftpfs.connection = easy;
  pthread_mutex_init(&ftpfs.lock, NULL);

  /* Set the filesystem name to show the current server */
  tmp = g_strdup_printf("-ofsname=curlftpfs#%s", ftpfs.host);
  fuse_opt_insert_arg(&args, 1, tmp);
  g_free(tmp);

  res = ftpfs_fuse_main(&args);

  cancel_previous_multi();
  curl_multi_cleanup(ftpfs.multi);
  curl_easy_cleanup(easy);
  curl_global_cleanup();
  fuse_opt_free_args(&args);

  pthread_mutex_destroy(&ftpfs.lock);
  cache_deinit();

  return res;
}
