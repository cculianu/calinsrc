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
#include <stdio.h>
#include <errno.h>
#include "common.h"
#include "tempfile.h"
#include "exception.h"


template <class T> class TempSpooler : public TempFile
{
 public:
  TempSpooler(const char * prefix = "ts", bool requireLocal = true)
    : TempFile(prefix, requireLocal), numTs(0) { updateNumTs(); }

  virtual ~TempSpooler() {}

  void spool(const T * thing, int nmemb) 
    { /* do spooling */ 
      int r;

      r = ::lseek(*this, 0, SEEK_END);
      Assert<FileException>(r >= 0, "Seek error in "__FILE__, strerror(errno));
      r = ::write(*this, thing, nmemb * sizeof(T));
      Assert<FileException>(r == nmemb * sizeof(T), "Could not write", strerror(errno));
      numTs += nmemb; 
      updateNumTs();
    }

  /* calls operation() for each T stored in the temp file */
  template<class OP> void forEach(OP & operation) 
    { 
      static const int num_at_a_time = 10;
      unsigned long long int numLeft = 0;
      int r;

      r = ::lseek(*this, 0, SEEK_SET);
      Assert<FileException>(r >= 0, "Seek error in "__FILE__, strerror(errno));
      r = ::read(*this, &numLeft, sizeof(numLeft));
      Assert<FileException>(r == sizeof(numLeft), "Read error", strerror(errno));

      char buf[sizeof(T) * num_at_a_time / sizeof(char)];
      int n_read;
      while( (n_read = ::read(*this, buf, sizeof(T) * num_at_a_time)) > 0 ) {
        int i;
        for (i = 0; i < n_read / sizeof(T); i++) 
          operation(reinterpret_cast<T *>(buf)[i]);        
        numLeft-=i;
      }
      
      if (numLeft) BUG();
    }

 private:
  unsigned long long int numTs;

  /* writes numTs as first sizeof(numTs) bytes in file.. auto-called in
     constructor and in spool() */
  void updateNumTs(void) 
    {
      int r;

      r = ::lseek(*this, 0, SEEK_SET);
      Assert<FileException>(r >= 0, "Seek error in "__FILE__, strerror(errno));
      r = ::write(*this, &numTs, sizeof(numTs));
      Assert<FileException>(r == sizeof(numTs), "Could not write", strerror(errno));
    }


};
