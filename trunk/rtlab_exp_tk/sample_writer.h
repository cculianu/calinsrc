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
#ifndef _SAMPLE_WRITER_H
# define _SAMPLE_WRITER_H

#include <qobject.h>
#include <zlib.h>
#include <set>
#include <vector>
#include "common.h"
#include "shared_stuff.h"
#include "sample_listener.h"

class SampleGZWriter: public QObject, public SampleListener
{
  Q_OBJECT

 public:
  SampleGZWriter();
  SampleGZWriter(const char *filename);
  ~SampleGZWriter();
  void setFile(const char *filename);
  void newSample(const SampleStruct *s);
  bool & periodicFlush() { return _periodicFlush; };

 public slots:
    
  void channelStateChanged(uint channel_id);
  void flushBuffer(); /* should be called by a QTimer? */

 private:
  void init();
  void putStateChangeInfo(const SampleStruct *s);

  gzFile file;
  set<int> channel_ids_that_have_a_committed_state;
  static const uint 
    BUFSIZE = 65536,  /* the number of bytes in our little buffer cache */
    STATE_CH_STRING_SIZE = 46, /* the size of the format string, 
				  sans null but with terminating newline */
    DATALINE_STRING_SIZE = 50; /* the size of each data line string, 
				  sans null but with terminating newline */
  static const char 
    * const stateChangeFormat = "State Changed: C[%03u] R[%010u] SI[%020llu]\n",
    * const dataLineFormat = "C[%03u] SI[%020llu] D[%#.14g]\n";
  char buffer[BUFSIZE+1];
  uint bufEnd; /* index of the first free pos in buffer (always <= BUFSIZE) */
  bool flushPending, /* if this is false, we need to schedule a flush */
       _periodicFlush; /* this needs to be true for the timed flushes to 
			  be enabled -- default is false */
};

#endif




























