/*
 * This file is part of the RT-Linux Multichannel Data Acquisition System
 *
 * Copyright (C) 1999,2000 David Christini
 * Copyright (c) 2001 David Christini, Lyuba Golub, Calin Culianu
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
#ifndef _COMEDI_COPROCESS_H
#define _COMEDI_COPROCESS_H

#include "comedi_device.h"
#include "shared_stuff.h"


/* 
   This class is a cheap hack to adapt the SharedMemStruct 'interface'
   that talks to the realtime module rt_process.o.  This class emulates
   rt_process.o in userspace.  It acts *just* like rt_prcoess,
   in that it's completely opaque and its only interface is SharedMemStruct.

   It internally creates a pthread when you call "start()" and 
   kills the pthread when you call "stop()".

   It is meant to be used from within DAQSystem::ReaderLoop

   ToDo: Make this more abstract -- make the rest of the application 
   and classes completely unaware of things like whether we are using 
   rt_process or whether we are doing it all in userspace.

   Currently we have 1 if statement inside ReaderLoop's constructor
   to determine if it shuld create this class and start() it or if it should
   attempt to attach to rt_process via the ShmController's utility method 
   attach().
*/
   
class ComediCoprocess : public SharedMemStruct
{
 public:
  ComediCoprocess (const ComediDevice & d);
  ~ComediCoprocess();
  void start();
  void stop();
  int fifoFd() { return ai_fifo_minor; /* ai_fifo_minor == pipe[0] */ }

 private:
  void threadLoop(); /*  do *not* call this from the main thread, EVER, as
                         it contains essentially a periodic, non-terminating
                         while(1) ! */

  void flushBuffer(); /* attempts to flush sample_buffer to pipe[1] */

  /* The below simply does: 
     reinterpret_cast<ComediCoprocess *>(arg)->threadLoop();
     return 0;
  */
  static void *pthread_friendly_threadLoop_wrapper(void *arg);

  static const int INITIAL_SAMPLING_RATE_HZ, INITIAL_CHANNEL_GAIN;
  ComediDevice device;
  comedi_t *ai_dev, *ao_dev;
  int pipe[2]; /* [0] is for reading, [1] is for writing */
  pthread_t thread;

  static const int sample_buffer_size;  
  SampleStruct *sample_buffer;
  int sbuf_i; /* index into next free pos in sample_buffer */
};





#endif
