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
#ifndef _SAMPLE_SPOOLER_H
#define _SAMPLE_SPOOLER_H
#include "sample_consumer.h"
#include "tempspooler.h"
#include "shared_stuff.h"

class SampleSpooler : public SampleConsumer, 
  protected TempSpooler<SampleStruct>
{
 public:
  SampleSpooler();
  ~SampleSpooler();
  
  void consume(const SampleStruct *);
  /* copies a range of samples in this temp spool to a SampleConsumer
     if start and end are 0, they are set to all the samples in the spool */
  void dump(SampleConsumer & destination, 
            scan_index_t start = 0, scan_index_t end = 0);

  bool isDirty() const { return dirty; }
  scan_index_t startIndex() const { return start_i; }
  scan_index_t endIndex() const { return end_i; }
  void flush(); /* flushes buffered samples to temp file */

 protected:
  /* resets this file to zero size */
  virtual void truncate();

 private:
  static const int sample_buffer_size = 100; /* number samples to buffer 
                                                before writing to disk */
  SampleStruct samples[sample_buffer_size];
  int n_buffer; /* when this reaches sample_buffer_size, flush to disk */

  scan_index_t start_i, end_i;

  bool dirty, uninit; 

  struct Dumper
  {
    Dumper(SampleConsumer &, scan_index_t start, scan_index_t end);    
    void operator()(const SampleStruct &);
    SampleConsumer & consumer;
    scan_index_t s, e;
  };

};

#endif
