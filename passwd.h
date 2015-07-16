#ifndef __CURLFTPFS_PASSWD_H__
#define __CURLFTPFS_PASSWD_H__

/*
    FTP file system
    Copyright (C) 2015 Vincent Pit <vince@profvince.com>

    This program can be distributed under the terms of the GNU GPL.
    See the file COPYING.
*/

int prompt_passwd(const char *what, char **userpwd);

#endif
