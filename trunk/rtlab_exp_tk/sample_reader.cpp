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

#include <string>
#include <strstream>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <string.h>
#include "sample_reader.h"
#include "sample_source.h"

/*----------------------------------------------------------------------------
  SampleStructReader method definitions
----------------------------------------------------------------------------*/
SampleStructReader::
SampleStructReader (SampleStructSource *source, int secs_to_block = -1)
  : secs_to_block_on_reads(secs_to_block)
{
  init();
  this->source = source;
}


SampleStructReader::~SampleStructReader() 
{
  uninit();
}

const SampleStructSource *
SampleStructReader::getSource() const
{
  return this->source;
}

void
SampleStructReader::newSource(SampleStructSource *source)
{
  uninit();
  init();
  this->source = source;  
}

const SampleStruct *
SampleStructReader::readAll()
{
  register uint num_samples_read, i;

  const SampleStruct *samples = source->read(secs_to_block_on_reads);

  num_samples_total += num_samples_read = source->numSamplesLastRead();

  num_dropped_last = 0;

  if (num_samples_read && num_samples_total <= num_samples_read) {
      /* we are in the first scan so record lowest scan index
	 that we see for future statistical usage */
    scan_started_index = samples->scan_index;
  }
  
  for (i = 0; i < num_samples_read; i++) {
    /* we are in a non-first pass, so we can rely on the
       current_scan_indices array, thus we can calculate
       whether we dropped any scans */
    if (channelSeenOnce[samples[i].channel_id]) {
      num_dropped_last += /* bump up num_dropped if we see a jump in 
			     scan index no.'s */
	samples[i].scan_index
	- current_scan_indices[samples[i].channel_id] - 1;
    } else {
      /* this is the first pass for this channel.  After this point
	 we will be able to rely on the record of current scan indices
	 for this channel */
      channelSeenOnce[samples[i].channel_id] = true;
    }
    
    /* update the record of all the current indices for each channel */
    current_scan_indices[samples[i].channel_id] = samples[i].scan_index;
    
  }
  num_dropped_total += num_dropped_last;
  
  return samples;
}

scan_index_t
SampleStructReader::numRead() const { return num_samples_total; }

scan_index_t
SampleStructReader::numDropped() const { return num_dropped_total; }

uint
SampleStructReader::numLastRead() const { return source->numSamplesLastRead();}

uint
SampleStructReader::numLastDropped() const { return num_dropped_last; }

/* returns the index of the first scan encountered */
scan_index_t 
SampleStructReader::firstScanIndex() const {  return scan_started_index; }

/* returns the index of the greatest scan encountered */
scan_index_t 
SampleStructReader::currentScanIndex() const 
{
  uint i;
  scan_index_t ret = 0;

  for (i = 0; i < SHD_MAX_CHANNELS; i++) {
    if (channelSeenOnce[i] && current_scan_indices[i] > ret)
      ret = current_scan_indices[i];
  }

  return ret;
}

/* protected default constructor */
SampleStructReader::SampleStructReader() 
{
  init();
}

void /* protected */
SampleStructReader::init()
{  

  /* just intialize some variables */
  source = NULL; /* dummy */

  /* clear out statistics */
  num_samples_total =  
  num_dropped_total =
  num_dropped_last =
  scan_started_index = 0;

  for (int i = 0; i < SHD_MAX_CHANNELS; i++) {
    channelSeenOnce[i] = false;
  }

}

/* shared destructor/de-initializer parts */
void /* protected */
SampleStructReader::uninit() 
{
  /* nothing */
}

