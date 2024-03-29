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
#include <qstring.h>
#include <qtimer.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "sample_writer.h"
#include "shm.h"

SampleWriter::SampleWriter(uint sampling_rate_hz) 
  : sampling_rate_hz(sampling_rate_hz), scan_index(0)
{ 
  _periodicFlush = flushPending = false; 
}

void SampleWriter::scanIndexChanged(scan_index_t newIndex)
{
  scan_index = newIndex;
}

void SampleWriter::samplingRateChanged(uint new_rate_hz)
{ 
  sampling_rate_hz = new_rate_hz; 
}

void SampleWriter::schedulePeriodicFlush()
{
  if (_periodicFlush && !flushPending) {
    /* wait an arbitrary 100 milliseconds before invoking the buffer flusher */
    QTimer::singleShot( 100, this, SLOT(flushBuffer()) );
    flushPending = true;
  }
}

SampleGZWriter::SampleGZWriter(uint srate, const char *filename)
  : SampleWriter(srate)
{
  init();
  if (filename) setFile(filename);
}


SampleGZWriter::~SampleGZWriter()
{
  if (file)
    gzclose(file);
}

void
SampleGZWriter::init()
{
  /* do init stuff here */
  file = NULL;
  bufEnd = 0;
  buffer[0] = 0;
  channel_ids_that_have_a_committed_state.clear();
}

void
SampleGZWriter::setFile(const char *filename)
{

  if (file != NULL) {
    if (bufEnd) flushBuffer();
    gzclose(file);
    file = NULL;
  }

  file = gzopen(filename, "wb1");
  if (file == NULL) {
    throw SampleOutputFileException
      (QString("Could not open ") + filename,
       QString("Error opening ") + filename + " for writing. Error is: \"" 
       + (errno ? sys_errlist[errno] 
          : "Not enough memory to allocate zlib data structures.")
       + "\""
      );				    
  }
}

void
SampleGZWriter::consume(const SampleStruct *s)
{
  if (file == NULL) {
    /* should maybe throw an exception? */
    return;
  }

  if (bufEnd + DATALINE_STRING_SIZE + STATE_CH_STRING_SIZE  > BUFSIZE) {
    flushBuffer(); /* worst case scenario is out buffer is full so we
		      need to commit it to disk using expensive flushBuffer()
		      which relies on expensive gzwrite() */
  }

  if (channel_ids_that_have_a_committed_state.find(s->channel_id) == 
      channel_ids_that_have_a_committed_state.end()) {
    putStateChangeInfo(s);
  }

  int n = 
    snprintf(buffer + bufEnd, BUFSIZE - (int) bufEnd, dataLineFormat, 
	     s->channel_id, s->scan_index, s->data);

  if (n < 0) {
    throw Exception ("Internal error.", "Buffer overflow...");
  }
  
  bufEnd += n;

  schedulePeriodicFlush();
}

void
SampleGZWriter::channelStateChanged(uint channel_id, bool on_or_off)
{
  if (on_or_off)
    channel_ids_that_have_a_committed_state.erase(channel_id);
}

void 
SampleGZWriter::flushBuffer()
{
  if (!file) {
    throw SampleOutputFileException("Sample Output File Not Defined",
				    "Sample Output File Not Defined.  "
				    "This is an internal application "
				    "error.  If you see this message, "
				    "the programmer was really confused "
				    "and used a library incorrectly.");    
  }

  if (bufEnd && !gzwrite(file, buffer, bufEnd)) {
    int errnum = 0;
    const char *errmsg = gzerror(file, &errnum);
    if (errnum == Z_ERRNO)
      errmsg = sys_errlist[errno];
    throw 
      SampleOutputFileException("Error writing to output file",
				QString("Zlib returned an error writing to the"
					" output file.  The error was: ") +
				errmsg);
  }
  
  buffer[(bufEnd = 0)] = 0;

  flushPending = false;
}

void
SampleGZWriter::putStateChangeInfo(const SampleStruct *s)
{ 
  int n =      
    snprintf(buffer + bufEnd, BUFSIZE - (int) bufEnd,
	     stateChangeFormat, s->channel_id, sampling_rate_hz, 
	     s->scan_index);
  if (n < 0) {
    throw Exception ("Internal error.", "Buffer overflow...");
  }
  bufEnd += n;
  channel_ids_that_have_a_committed_state.insert
    (channel_ids_that_have_a_committed_state.end(), s->channel_id);
}



SampleBinWriter::SampleBinWriter (uint srate, const char * file) 
  : SampleWriter(srate)
{
  if (file)
    setFile(file);
}

SampleBinWriter::~SampleBinWriter () 
{ /* not necessary ... dsdostream.end(); */ };

void SampleBinWriter::setFile(const char *filename)
{
  dsdostream.setOutFile(filename, sampling_rate_hz);
}

void SampleBinWriter::consume(const SampleStruct *s)
{
  if (s->scan_index > scan_index) scanIndexChanged(s->scan_index);
  dsdostream.writeSample(s);
}

void SampleBinWriter::channelStateChanged(uint channel_id, bool on_or_off = true)
{
  if (!on_or_off) dsdostream.removeChannelAfter(channel_id, scan_index+1);
}

