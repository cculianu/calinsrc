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

class TempFile 
{
public:
  TempFile(const char * prefix = "tt", bool requireLocal = true);
  virtual ~TempFile();
  
  int fd() { return _fd; }
  void copyTo(int dest_fd);
  void copyTo(const char *fileName); /* opens a file O_WRONLY|O_CREAT|O_TRUNC 
                                        and calls copyTo(int) on the filedes */

  operator int() { return fd(); } /* returns the temp file fd */
  TempFile & operator>>(int fd) { copyTo(fd); } /* appends temp file to fd */
  TempFile & operator>>(const char *f) { copyTo(f); } 

private:

  int _fd;

  static const char * env_vars_to_try[], 
                    * default_tmp_dir;

};

