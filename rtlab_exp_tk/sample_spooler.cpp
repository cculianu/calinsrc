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
#include "sample_spooler.h"
#include <qmessagebox.h>
#include <iostream>

SampleSpooler::SampleSpooler()
  : TempSpooler<SampleStruct>("daq", true)
{
  truncate(); /* cheesy way to reset all values to defaults */
}

SampleSpooler::~SampleSpooler()
{ flush(); /* just to be anal */ }

void SampleSpooler::consume(const SampleStruct *s)
{
  samples[n_buffer++] = *s;

  if (n_buffer == sample_buffer_size) flush();

  if (uninit) { start_i = s->scan_index; uninit = false; }
  
  if (end_i < s->scan_index) end_i = s->scan_index;

  dirty = true;
}

void SampleSpooler::flush()
{
  unsigned long long int orig_ns = numSpooled();

  try {
    spool(samples, n_buffer);
  } catch (DiskFullException & e) {     
    uint64 numDropped = numSpooled() + (n_buffer - (numSpooled() - orig_ns));

    truncate();    
    cerr << __FILE__ << ", line " << __LINE__ 
         << ": Out of temporary storage. " << "Temporary storage exhausted.  "
         << "As a result, " <<  uint64_to_cstr(numDropped) 
         << " samples were lost." << endl;
    
    return;
  }
  n_buffer = 0;
}

SampleSpooler::Dumper::Dumper(SampleConsumer & c, scan_index_t start,  scan_index_t end) : consumer(c), s(start), e(end)
{}

void SampleSpooler::Dumper::operator()(const SampleStruct & s)
{
  consumer.consume(&s);
}


void SampleSpooler::dump(SampleConsumer & c, scan_index_t s, scan_index_t e)
{
  if (s == 0 && e == 0) { s = startIndex(); e = endIndex(); }
  
  flush();

  Dumper d(c, s, e);
  forEach(d);

  dirty = false;
}

void SampleSpooler::truncate()
{ 
  start_i = end_i = 0; 
  n_buffer = 0;
  uninit = true; 
  dirty = false; 
  TempSpooler<SampleStruct>::truncate(); 
}
