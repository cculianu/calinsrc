/*
 * This file is part of the RT-Linux Multichannel Data Acquisition System
 *
 * Copyright (C) 2001 Calin Culianu
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

#ifndef _SAMPLE_READER_H
# define _SAMPLE_READER_H

#include <string>
#include "shared_stuff.h"
#include "sample_source.h"

/* todo: rethink OO-ness of this -- ideally we want a generic 
   reader interface that can work with any data source -- including
   comedi user-space drivers!
   -- update for tomorrow: I wrote some 'SampleStructSource' classes to be 
      generically used with this reader class.  No need to have that 
      messy 'fd' stuff! 
*/

class SampleStructReader {
 public:
  SampleStructReader (SampleStructSource *source, int blocking_time_secs = -1);

  ~SampleStructReader();

  /* returns the current reader's source */
  const SampleStructSource * getSource() const;

  /* re-sets the sample source for this instance to something else..
     all statistics get re-set as well */
  void newSource(SampleStructSource *source);

  /* the time in seconds that reads take to block.  A negative value
     here indicates infinite blocking time! */
  int readBlockTime() const { return secs_to_block_on_reads; };
  int &readBlockTime() { return secs_to_block_on_reads; };

  /* reads all the samples available, and returns a pointer
     to an array of them.  To check the size of this array, use
     numRead().  The array returned is an internal data structure and
     is not persistent across calls to readAll().

     This is a blocking read.
  
     SampleDeviceEOFException is thrown on end-of-file
  */
  const SampleStruct *readAll();

  /* returns the total number of samples read (without errors) 
     since the use of the current source */
  unsigned long long numRead() const;

  /* returns the total number of dropped samples since this object has
     read from this source */
  unsigned long long numDropped() const;

  /* the number of samples last read */
  uint numLastRead() const;
  
  /* the of samples dropped at the last read */
  uint numLastDropped() const;

  /* returns the index of the smallest scan encountered */
  scan_index_t firstScanIndex() const;

  /* returns the index of the greatest scan encountered */
  scan_index_t currentScanIndex() const;

 protected:
  SampleStructReader();
  void init();
  void uninit();

  /* statistics properties */
  
  scan_index_t current_scan_indices[SHD_MAX_CHANNELS], scan_started_index,
               num_samples_total, num_dropped_total, num_dropped_last;
  bool channelSeenOnce[SHD_MAX_CHANNELS];
  /* a negative number here indicates that reads block indefinitely */
  int secs_to_block_on_reads;
  
 private:  
  SampleStructSource *source;

};



#endif
