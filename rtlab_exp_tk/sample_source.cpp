/*
 * This file is part of the RT-Linux Multichannel Data Acquisition System
 *
 * Copyright (C) 1999,2000 David Christini
 * Copyright (C) 2001 David Christini, Lyuba Golub, Calin Culianu
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
#include <string>
#include <strstream>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <string.h>
#include <qdatetime.h>
#include "exception.h"
#include "sample_source.h"
#include "shm.h"
#include "shared_stuff.h"

/*----------------------------------------------------------------------------
  SampleStructSource method definitions
----------------------------------------------------------------------------*/

SampleStructSource::SampleStructSource() 
{ 
  read_memory = NULL;
  read_memory_sz = num_bytes_last_read = 0;
}

SampleStructSource::~SampleStructSource()
{
  if (read_memory) delete read_memory;
}

size_t
SampleStructSource::numBytesLastRead() const
{
  return num_bytes_last_read;
}

uint
SampleStructSource::numSamplesLastRead() const
{
  return numBytesLastRead() / SAMPLE_SOURCE_BLOCK_SZ_BYTES;
}

SampleStructFileSource::SampleStructFileSource (const string & filename)
{
  open(filename);
  i_opened_fd = true;
}

SampleStructFileSource::SampleStructFileSource (unsigned int filedes)
  : fd(filedes)
{
  i_opened_fd = false;
}

SampleStructFileSource::~SampleStructFileSource ()
{
  if (i_opened_fd)  close();
}

void
SampleStructFileSource::waitForNewData(int num_secs = -1)
{
  struct timeval tv;
  fd_set rfds;

  tv.tv_usec = 0;
  tv.tv_sec = num_secs;
  FD_ZERO(&rfds);
  FD_SET(fd, &rfds);

  /* block num_secs time waiting for data, or
     indefinitely if num_secs is less than zero */
  errno = 0;
  int ret = select (fd+1, &rfds, NULL, NULL, (num_secs < 0 ? NULL : &tv));

  if ( ret < 0 && errno != EINTR && errno != ENOMEM && numBytesReady() == 0 ) {
    throw SampleDeviceEOFException(); // dead reader device?
  } else if (errno == ENOMEM) {
    throw "out of memory on select()"; /* need to handle this better */
  }
}

size_t
SampleStructFileSource::numBytesReady() const
{
  int size;

  if ( ioctl(fd, FIONREAD, &size) < 0 ) {
    throw SampleDeviceException();
  }

  return size;
}

int
SampleStructFileSource::numSamplesReady() const
{
  return numBytesReady() / SAMPLE_SOURCE_BLOCK_SZ_BYTES;
}

const SampleStruct *
SampleStructFileSource::read(int num_secs_wait = -1)
{
  int ret;

  /* this variable name is a misnomer here since within this method
     it represents data to be read, that wasn't read yet.. */
  num_bytes_last_read = numBytesReady();

  /* the select() in waitForNewData() seems to hang forever? */
  if (num_bytes_last_read == 0) {
    waitForNewData(num_secs_wait);
    /* the select() inside waitForNewData timed out, so cop out early */
    if (! (num_bytes_last_read = numBytesReady())) return  read_memory;
  }


  if (num_bytes_last_read > read_memory_sz) {
    if (read_memory) delete read_memory;
    read_memory_sz = num_bytes_last_read;
    read_memory = (SampleStruct *)new char[read_memory_sz];
  }


  
  if ( (ret = ::read(fd, read_memory, num_bytes_last_read)) < 0) {
    int saved_errno = errno;
    switch (saved_errno) {    
    // maybe some custom errors here?
    default:
      throw SampleDeviceException();
      break;
    }
  } else if (num_bytes_last_read != 0 && ret == 0) {
    throw SampleDeviceEOFException();
  }

  return read_memory;
    
}

void
SampleStructFileSource::open(const string & filename)
{
  fd = ::open (filename.c_str(), O_RDONLY);
  if ( fd  < 0 ) {
    throw SampleDeviceException();
  }
}

void 
SampleStructFileSource::close()
{
  ::close(fd);
}

SampleStructFIFOSource::SampleStructFIFOSource (unsigned int filedes = 0) 
  : SampleStructFileSource(filedes)
{}

SampleStructFIFOSource::SampleStructFIFOSource (const string & filename)
  : SampleStructFileSource(filename)
{}

SampleStructFIFOSource::~SampleStructFIFOSource () {}

void
SampleStructFIFOSource::flush() {
  
  read(0);    
}

