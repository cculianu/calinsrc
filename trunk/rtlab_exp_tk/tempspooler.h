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
#include <stdio.h>
#include <errno.h>
#include <zlib.h>
#include "common.h"
#include "tempfile.h"
#include "exception.h"

#ifndef _TEMP_SPOOLER_H
#define _TEMP_SPOOLER_H
template <class T> class TempSpooler : public TempFile
{
 public:
  TempSpooler(const char * prefix = "ts", bool requireLocal = true)
    : TempFile(prefix, requireLocal), numTs(0) {}

  virtual ~TempSpooler() {}

  virtual void truncate() { ftruncate(*this, 0); updateNumTs(0); }

  virtual unsigned long long int numSpooled() const { return numTs; }

  virtual void spool(const T * thing, int nmemb) 
    { /* do spooling */ 
      int r;

      updateNumTs(numTs + nmemb);

      r = ::lseek(*this, 0, SEEK_END);
      Assert<FileException>(r >= 0, "Seek error in "__FILE__, strerror(errno));
      r = ::write(*this, thing, nmemb * sizeof(T));
      if ( r != static_cast<int>(nmemb * sizeof(T) ) ) {
        int saved_errno = errno;
        switch (saved_errno) {
        case EFBIG:
        case ENOSPC:
          throw DiskFullException("The disk is full.", strerror(saved_errno));
          break;
        default:
          throw FileException ("Could not write", 
                               QString("Error writing to a temporary file.  ") 
                               + strerror(errno));
          break;
        }
      }
    }

  /* calls operation() for each T stored in the temp file */
  template<class OP> void forEach(OP & operation) 
    { 
      static const int num_at_a_time = 10;
      unsigned long long int numLeft = 0;
      int r;

      if (!numSpooled()) return; /* abort early if we have no data */

      r = ::lseek(*this, 0, SEEK_SET);
      Assert<FileException>(r >= 0, "Seek error in "__FILE__, strerror(errno));
      r = ::read(*this, &numLeft, sizeof(numLeft));
      Assert<FileException>(r == sizeof(numLeft), "Read error", strerror(errno));

      char buf[sizeof(T) * num_at_a_time / sizeof(char)];
      int n_read;
      while( (n_read = ::read(*this, buf, sizeof(T) * num_at_a_time)) > 0 ) {
        int i;
        for (i = 0; i < static_cast<int>(n_read / sizeof(T)); i++) 
          operation(reinterpret_cast<T *>(buf)[i]);        
        numLeft-=i;
      }
      
      if (numLeft) BUG();
    }

 private:
  unsigned long long int numTs;

  /* writes numTs as first sizeof(numTs) bytes in file.. auto-called in
     constructor and in spool() */
  void updateNumTs(unsigned long long int newNumTs) 
    {
      int r;
      off_t orig = ::lseek(*this, 0, SEEK_CUR);
      
      numTs = newNumTs;

      r = ::lseek(*this, 0, SEEK_SET);
      Assert<FileException>(r >= 0, "Seek error in "__FILE__, strerror(errno));
      r = ::write(*this, &numTs, sizeof(numTs));
      Assert<FileException>(r == sizeof(numTs), "Could not write", strerror(errno));
      ::lseek(*this, orig, SEEK_SET); // go back to where we were...
    }


};

template <class T> class TempSpoolerGZ : public TempSpooler<T>
{

 private:
  enum Mode { Read, Write };

 public:
  TempSpoolerGZ(const char * prefix = "gzts", bool requireLocal = true)
    : TempSpooler<T>(prefix, requireLocal), num_spooled(0)
    { truncate(); openGZ(); }
  virtual ~TempSpoolerGZ() { closeGZ(); }

  virtual unsigned long long int numSpooled() const { return num_spooled; }

  virtual void spool(const T * thing, int nmemb) 
    { /* do spooling */ 
      if ( gzwrite(gzfile, const_cast<T *>(thing), 
                   sizeof(T) * nmemb) != sizeof(T) * nmemb ) {
        int err;
        throw FileException ("Could not write", 
                             QString("Error writing to a temporary gz file.  ")
                             + gzerror(gzfile, &err));
      }
      num_spooled += nmemb;
    }

  /* calls operation() for each T stored in the temp file */
  template<class OP> void forEach(OP & operation) 
    { 
      static const int num_at_a_time = 10;
      unsigned long long int numLeft = numSpooled();
      int r;

      if (!numSpooled()) return; /* abort early if we have no data */

      closeGZ();
      openGZ(Read);
      
      Assert<FileException>(r >= 0, "Seek error in "__FILE__, gzerror(gzfile, 
                                                                      &errno));

      char buf[sizeof(T) * num_at_a_time / sizeof(char)];
      int n_read;
      while((n_read = ::gzread(gzfile, buf, sizeof(T) * num_at_a_time)) != 0){
        Assert<FileException>(n_read > 0, 
                              "Read error in "__FILE__, 
                              gzerror(gzfile, &errno));
        int i;
        for (i = 0; i < static_cast<int>(n_read / sizeof(T)); i++) 
          operation(reinterpret_cast<T *>(buf)[i]);        
        numLeft-=i;
      }
      
      if (numLeft) BUG();

      closeGZ();
      openGZ(Write);
      ::gzseek(gzfile, 0, SEEK_END);
    }
  
  
 private:
  gzFile gzfile;
  unsigned long long num_spooled;
  void openGZ(Mode m = Write) 
    { 
      ::lseek(*this, 0, SEEK_SET);
      gzfile = gzdopen(dup(*this), m == Write ? "wb" : "rb"); 
    }
  void closeGZ() { gzclose(gzfile); }
};
#endif
