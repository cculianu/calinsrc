/*! AVN Stim - The kernel side defs.. */

/*
 * This file is part of the RT-Linux Multichannel Data Acquisition System
 *
 * Copyright (C) 1999,2000 David Christini
 * Copyright (C) 2001,2002 Calin Culianu
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (see COPYRIGHT file); if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA, or go to their website at
 * http://www.gnu.org.
 */
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/vfs.h> /* for statfs() */
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

#include "tempfile.h"

#include "exception.h"


/* TODO: Add more error detection/correction from system calls */

/* this should never change so that's why I kept it hardcoded rather
   than use an #include */
#define NFS_SUPER_MAGIC                 0x6969
#define CODA_SUPER_MAGIC                0x73757245

const char *TempFile::env_vars_to_try[] = {"TMP", "TMPDIR", 0};
const char *TempFile::default_tmp_dir  = "/tmp";

TempFile::TempFile(const char * prefix, bool requireLocal) 
  : _fd(-1)
{
  static const char *Xs = "XXXXXX";

  const char ** cur = env_vars_to_try;
  const char  * dir = 0;
  struct statfs stbuf;

  while (*cur && !dir) {
    const char * direnv = getenv(*cur);        
    if ( direnv &&                                 
         (!requireLocal 
          || !statfs(direnv, &stbuf) &&
          stbuf.f_type != NFS_SUPER_MAGIC &&
          stbuf.f_type != CODA_SUPER_MAGIC) ) 
      /* environment variable is found, the dir exits, it is on a valid,
         local fs, and all is right with the world! */
      dir = direnv;
    cur++;
  }
  if (!dir || !*dir) dir = default_tmp_dir;

  int len = strlen(dir) + 1 + strlen(prefix) + strlen(Xs) + 1;

  char filename[len];

  snprintf(filename, len, "%s/%s%s", dir, prefix, Xs);
  
  Assert<FileCreationException>((_fd = ::mkstemp(filename)) >= 0, 
                                "Could not create temp file",
                                QString(filename) + " could not be created: " +
                                strerror(errno));
  
  ::unlink(filename); /* in case we crash, Unixey OS's will delete the file */
}

TempFile::~TempFile()
{
  if (_fd >= 0) ::close(_fd);
}

void TempFile::copyTo(int dest_fd)
{

  ::lseek(fd(), SEEK_SET, 0);
  
  char * buf = new char[4096]; /* makes sure we don't use stack */
  int n_read;

  while ( (n_read = ::read(fd(), buf, 4096)) > 0 ) 
    if ( ::write(dest_fd, buf, n_read) < 0) break;

  delete buf;
}

void TempFile::copyTo(const char *f)
{
#ifndef O_LARGEFILE
#define O_LARGEFILE 0
#endif
  int fd = ::open(f, O_WRONLY | O_CREAT | O_TRUNC | O_LARGEFILE, 0666);

  copyTo(fd);

  ::close(fd);
}
