/***************************************************************************
                          scanproc.c  -  Utility functions for /proc scanning
                             -------------------
    begin                : Thu Feb 7 2002
    copyright            : (C) 2002 by Calin Culianu
    email                : calin@rtlab.org
 ***************************************************************************/
 
/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#include "scanproc.h"
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h> 
#include <limits.h>
#include <errno.h>
#include <string.h>

#define PROCPATH "/proc"


static int have_proc_fs(void);
static int select_numeric_dir(const struct dirent *);
static int countlist(void **);

static int HAVEPROC_ = -1;
#define HAVEPROC (HAVEPROC_ > -1 ? HAVEPROC_ : (HAVEPROC_ = have_proc_fs()))
#define CHKPROC do { if (!HAVEPROC) return 0; } while (0)

static int have_proc_fs(void)
{
  struct stat buf;
  char pself[PATH_MAX+1];
  
  pself[PATH_MAX] = 0;

  snprintf(pself, PATH_MAX, "%s/%s", PROCPATH, "self");
  
  return ( !stat(pself, &buf)  && S_ISDIR(buf.st_mode) );
}

static int select_numeric_dir(const struct dirent *d)
{
  char *endptr = d->d_name;

  strtol(d->d_name, &endptr, 10);
  
  /* if the end is non-null.. means the string containes some non-numeric
     characters.. thus we reject this directory as it can't be a pid */
  if (*(d->d_name) && *endptr) return 0;
  
  /* return true -- this is a numeric dir */
  return 1;
}

/* returns NULL on major error, or a malloc'd pointer to a zero-terminated
   pid_t array (which may itself be of length 0) on success */
pid_t *pids_of_my_exe(void)
{
  char buf[PATH_MAX+1], myexe[PATH_MAX+1];
  int ret;

  CHKPROC;

  buf[PATH_MAX] = 0;

  /* build the /proc/self/exe string */
  snprintf(buf, PATH_MAX, "%s/%s/%s", PROCPATH, "self", "exe");
  
  ret = readlink(buf, myexe, PATH_MAX);

  /* this should never happen.. */
  if (ret < 0) { perror("readlink"); return 0; }

  myexe[ret] = 0;
  
  return pids_of_exe(myexe);
}

/* returns NULL on major error, or a malloc'd pointer to a zero-terminated
   pid_t array (which may itself be of length 0) on success */
pid_t *pids_of_exe(const char *exe)
{
  struct dirent **dirs;
  pid_t *ret, *pcur;
  int n;

  CHKPROC;

  n = scandir(PROCPATH, &dirs, select_numeric_dir, alphasort);

  /* couldn't scan /proc? hmm.. should never happen */
  if (n < 0) { perror("scandir"); return 0; }

  ret = pcur = (pid_t *)calloc(n+1, sizeof(pid_t));
  
  while(n--) {
    char buf[PATH_MAX+1], link[PATH_MAX+1];
    int sz;

    snprintf(buf, PATH_MAX, "%s/%s/exe", PROCPATH, dirs[n]->d_name);

    sz = readlink(buf, link, PATH_MAX);
    
    /* sz < 0 ... the process either no longer exists, or is not owned by us 
       so try and check /proc/PID/maps instead.. the first map seems to 
       always be the executable! (this is a hack!) */
    if (sz < 0) {
      char mapfile[PATH_MAX+1], *protect_from_overflow;
      FILE *f;

      sz = 0;
    
      snprintf(mapfile, PATH_MAX, "%s/%s/maps", PROCPATH, dirs[n]->d_name);
      mapfile[PATH_MAX] = 0;
      f = fopen(mapfile, "r");

      if (!f) continue; /* ok we give up.. forget this /proc/PID entry! */

      /* parse /proc/PID/maps.. sixth column is mapping name */
      if (fscanf(f, "%*s %*s %*s %*s %*s %as", &protect_from_overflow) == 1) {
        strncpy(link, protect_from_overflow, PATH_MAX);
        sz = PATH_MAX; /* so code below don't break... */
        free(protect_from_overflow);
      }
      fclose(f);
	}

    link[sz] = 0; /* add null.. damned readlink() */

    /* 
       At this point, 
       link is either /proc/PID/exe's destination OR
       the first mapping entry in /proc/PID/maps!

       If link matches the requested executable, 
       append PID to the pid_t return list... 
    */
    if (!strncmp(link, exe, PATH_MAX)) {
      *pcur = strtol(dirs[n]->d_name, 0, 10); /* we know it's numeric.. */
      pcur++;
    }
  }
  *pcur = 0;

  free(dirs);

  return ret;
}

int num_procs_of_my_exe(void)
{
  pid_t *pids = pids_of_my_exe();
  int ret;

  ret = countlist((void **)pids);
  if (pids) free(pids);

  return ret; 
}

int num_procs_of_exe(const char *exe)
{
  pid_t *pids = pids_of_exe(exe);
  int ret;

  ret = countlist((void **)pids);
  if (pids) free(pids);

  return ret; 
}

char * grab_full_cmd_name_of_pid(pid_t pid, int *sz)
{
  char cmdfile[PATH_MAX+1];
  FILE *f;
  char *ret = 0;
  
  snprintf(cmdfile, PATH_MAX, "%s/%d/cmdline", PROCPATH, pid);  
  cmdfile[PATH_MAX] = 0;         
  
  f = fopen(cmdfile, "r");
  if (!f) { /* silently ignore dead procs.. */ return ret; }
  
  fscanf(f, "%as", &ret); 
  fclose (f);
  
  /* optionally compute the size... */
  if (sz)  *sz = (ret ? strlen(ret) : 0);
  
  return ret;
}

char * grab_stripped_cmd_name_of_pid(pid_t pid, int *sz)
{
  char 
    *fullcmd = grab_full_cmd_name_of_pid(pid, sz),
    *ret = 0, *cur = 0;

  if (!fullcmd) goto out;

  cur = strrchr(fullcmd, '/');
  if (cur) {
    cur++; /* consume the '/' */
  } else {
    cur = fullcmd;
  }

  ret = (char *)calloc(strlen(cur), sizeof(char));

  strcpy(ret, cur); 
  
 out:
  if (fullcmd) free(fullcmd);
  return ret;
}

char * grab_my_full_cmd_name(int *sz)
{
  return grab_full_cmd_name_of_pid(getpid(), sz);
}

char * grab_my_stripped_cmd_name(int *sz)
{
  return grab_stripped_cmd_name_of_pid(getpid(), sz);
}


static int countlist(void **list)
{
  int ret = 0;

  while (list && *(list++)) ret++;
  
  return ret;
}
