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
#include "shm_mgr.h"
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
  init(filename);
}

SampleStructFileSource::~SampleStructFileSource ()
{
  destruct();
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
SampleStructFileSource::init(const string & filename)
{
  open(filename);
}

void 
SampleStructFileSource::destruct()
{
  close();
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

SampleStructRTFSource::SampleStructRTFSource (unsigned short minor = 0) 
{
  strstream s;

  s << "/dev/rtf" << minor;
  
  string filename(s.str());
  init (filename);
}

void
SampleStructRTFSource::flush() {
  
  read(0);  
  
}

const SampleStruct * 
SampleStructRTFSource::read(int b_time=-1)
{
  pollWaitTimer.start();
  return SampleStructFileSource::read(b_time);
}

int 
SampleStructRTFSource::suggestPollWaitTime() const 
{
  int sleeptime_ms, n_read, proc_time_ms;
  uint n_chans_in_use;

  proc_time_ms = pollWaitTimer.elapsed();
  sleeptime_ms = DESIRED_FIFO_FEEL_MS - proc_time_ms;  
  n_chans_in_use = ShmMgr::numChannelsInUse(ComediSubDevice::AnalogInput);
  n_read = numSamplesLastRead();

  /* Thus far our 'period' is DESIRED_FIFO_FEEL_MS - proc_time_ms,
     but now check for negative! */
  if ( (sleeptime_ms) < 0 ) sleeptime_ms = 0;
 
  /* fixme: this code is more or less not really too helpful.  It is just
     complex and seems to provide little benefit above a constant 1 ms sleep
     time.  The idea was to minimize CPU usage by reading from the sample
     reader only when enough data acrues... but in actualiy I can't
     see any benefit to this code.. :(

     Todo: determine if this logic actually helps keep cpu usage down
  */

  /* if we have channels... */
  if (n_chans_in_use) {
    double samples_per_ms, ms_per_proc;
    
    /* collect some stats... */
    samples_per_ms = n_chans_in_use * (ShmMgr::shm->sampling_rate_hz / 1000.0);
    ms_per_proc = proc_time_ms / ( /* no 0's */ n_read ? n_read : 1.0 );

    if ( samples_per_ms == 0.0 ) /* prevent divide by 0 */ 
      samples_per_ms = 1/(double)ShmMgr::shm->sampling_rate_hz; 

    /* is sleeptime way too small? (we won't even get 1 sample in that time)
       so we might as wait sleep until we think we will have a sample */
    if ( samples_per_ms * sleeptime_ms < 1 ) 
      sleeptime_ms = (int) (1 / samples_per_ms);
    
    
    /* is it too big? (will we fill our buffer past the 10% watermark?) */
    if ( sleeptime_ms * samples_per_ms > (RT_QUEUE_SZ_BLOCKS * 0.50) ) {
      /* we want to sleep just enough to fill 10% of the buffer per sleep 
	 cycle, if possible */
      sleeptime_ms = (int) ( RT_QUEUE_SZ_BLOCKS * 0.10 * (1/samples_per_ms) 
			     -  RT_QUEUE_SZ_BLOCKS * 0.10 * ms_per_proc );
      sleeptime_ms = ( sleeptime_ms < 0 ? 0 : sleeptime_ms );
    }

#ifdef DAQ_SYSTEM_PROFILE_SLEEPTIME_CODE
    {
      static int max_sleeptime = sleeptime_ms, min_sleeptime = max_sleeptime; 
      static double avg_sleeptime = sleeptime_ms;
      static unsigned long long idx=1;
      
      if ( sleeptime_ms > max_sleeptime) max_sleeptime = sleeptime_ms;
      if ( sleeptime_ms < min_sleeptime) min_sleeptime = sleeptime_ms;
      avg_sleeptime += (sleeptime_ms - avg_sleeptime) / ((double)idx);
      
      if (idx % (1000 / DESIRED_FIFO_FEEL_MS) == 0) {
	cerr << "Idx: " << idx << " Sleeptime: " << sleeptime_ms 
	     << " Avg: " << avg_sleeptime  << " Max: " << max_sleeptime 
	     << " Min: " << min_sleeptime << endl;
      }
      idx++;
    }
#endif
  } /* end if (we have channels) */
  return sleeptime_ms;
}
