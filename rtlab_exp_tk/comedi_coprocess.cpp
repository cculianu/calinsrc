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
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/time.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h> 
#include <pthread.h>

#include <comedilib.h>


#define SHARED_MEM_STRUCT_SUBCLASS
#include "shared_stuff.h"
#undef SHARED_MEM_STRUCT_SUBCLASS

#include "comedi_coprocess.h"
#include "exception.h"
#include "comedi_device.h"

const int ComediCoprocess::sample_buffer_size = 100;

static inline void microsleep(long time_us);

const int ComediCoprocess::INITIAL_SAMPLING_RATE_HZ = 1000,
          ComediCoprocess::INITIAL_CHANNEL_GAIN = 0;

ComediCoprocess::ComediCoprocess(const ComediDevice & d) 
  : device(d),  ai_dev(0),   ao_dev(0), 
    thread(0), sample_buffer(0), sbuf_i(0) /* i hope pthread_t is an int! */
{
  Assert<SystemResourceException>
    (!::pipe(pipe), "INTERNAL ERROR: Could not create pipe.",
     QString("System call to pipe() returned ") + "an error: " 
     + strerror(errno) + " in " __FILE__);

  //  fcntl(pipe[1], F_SETFL, O_NONBLOCK); /* is this safe? */
  
  struct_version = SHD_SHM_STRUCT_VERSION;
  ai_minor = device.minor;
  ao_minor = device.minor;

  const ComediSubDevice &aiSubDev = device.find(ComediSubDevice::AnalogInput),
                        &aoSubDev = device.find(ComediSubDevice::AnalogOutput);
  
  ai_subdev = aiSubDev.id;
  ao_subdev = aoSubDev.id;;

  ai_fifo_minor = pipe[0]; /* cheap hack */
  ao_fifo_minor = -1;
  /* num AI channels in use */
  n_ai_chans = aiSubDev.n_channels; 
  n_ao_chans = aoSubDev.n_channels;
  sampling_rate_hz = INITIAL_SAMPLING_RATE_HZ;
  scan_index = 0;

  /* initialize the spike_params member */
  init_spike_params(&spike_params);
  
  for (uint i = 0; i < n_ai_chans || i < n_ao_chans; i++) { 
    /* set channel parameters */   
    if (i < n_ai_chans) {
      ai_chan[i] = CR_PACK(i,INITIAL_CHANNEL_GAIN,AREF_GROUND);
      set_chan(i, ai_chans_in_use, 0); 
    }
    
    if (i < n_ao_chans)  set_chan(i, ao_chans_in_use, 0);
    
  }

  sample_buffer = new SampleStruct[sample_buffer_size];

  /* nothing */
}

ComediCoprocess::~ComediCoprocess()
{  
  stop();

  close(pipe[0]); close(pipe[1]);
  if (sample_buffer) { delete sample_buffer; sample_buffer = 0; }
}


void *
ComediCoprocess::pthread_friendly_threadLoop_wrapper(void *arg)
{
  reinterpret_cast<ComediCoprocess *>(arg)->threadLoop();
  return 0;
}

void ComediCoprocess::start()
{
  stop(); /* kill any already-running threads */

  int err = 
    pthread_create(&thread, 0, pthread_friendly_threadLoop_wrapper, 
                   reinterpret_cast<void *>(this));
  
  Assert<SystemResourceException>
    (!err, 
     "INTERNAL ERROR: Thread creation problebm.", QString(
     "Could not create the comedi device access thread! The associated error "
     "message is:\n") + strerror(err) + "\nException thrown in " __FILE__);
}

void ComediCoprocess::stop()
{
  if (thread) {
    pthread_cancel(thread);
    pthread_join(thread, 0);
    thread = 0;
  } 
}


void 
ComediCoprocess::threadLoop()
{
  struct timeval loop_begin, loop_end;
  long looptime_us = 0, period_us = 0;
  uint chan;
  comedi_insn insn[n_ai_chans];
  comedi_insnlist insn_list;
  lsampl_t databuf[n_ai_chans];
  SampleStruct tmp_sample;
  vector<comedi_range> ranges = device.find(ComediSubDevice::AnalogInput).ranges();
  lsampl_t maxdata = device.find(ComediSubDevice::AnalogInput).maxData();

  insn_list.insns = insn;

  while(1) {
    gettimeofday(&loop_begin, 0);

    pthread_testcancel();        
   
    for (insn_list.n_insns = 0, chan = 0; chan < n_ai_chans; chan++) {
      if (is_chan_on(chan, ai_chans_in_use)) {
        insn[insn_list.n_insns].insn = INSN_READ;
        insn[insn_list.n_insns].n = 1;
        insn[insn_list.n_insns].data = databuf + chan;
        insn[insn_list.n_insns].subdev = ai_subdev;
        insn[insn_list.n_insns].chanspec = ai_chan[chan]; 
        insn_list.n_insns++;
      }      
    }

    comedi_do_insnlist(ai_dev, &insn_list);

    for (uint i = 0; i < insn_list.n_insns; i++) {
      tmp_sample.scan_index = scan_index;
      tmp_sample.channel_id = CR_CHAN(insn[i].chanspec);
      tmp_sample.data = comedi_to_phys(databuf[tmp_sample.channel_id], &ranges[CR_RANGE(insn[i].chanspec)], maxdata);
      tmp_sample.spike = 0; // hack for now
      tmp_sample.spike_period = 0;
      if (sbuf_i < sample_buffer_size) 
	sample_buffer[sbuf_i++] = tmp_sample; 
    }

    flushBuffer();
    
    period_us = 1000000 / sampling_rate_hz;

    scan_index++;

    gettimeofday(&loop_end, 0);

    looptime_us =  (loop_end.tv_sec * 1000000 + loop_end.tv_usec) - 
                   (loop_begin.tv_sec * 1000000 + loop_begin.tv_usec);
    
    if (period_us > looptime_us) microsleep(period_us - looptime_us);
  }

}

/* busy sleep */
inline void microsleep(long time_us)
{
  struct timeval begin, now;

  gettimeofday(&begin, 0);

  do {
    gettimeofday(&now, 0);
  } while ( (now.tv_sec * 1000000 + now.tv_usec) 
            - (begin.tv_sec * 1000000 + now.tv_usec) <= time_us);
  
}


void ComediCoprocess::flushBuffer()
{
  if (sbuf_i >= sample_buffer_size/4) { /* do write on 1/4 buffer fill? */

    /* assumption is a blocking write */
    int bytes_written = 
      write(pipe[1], sample_buffer, sbuf_i * sizeof(SampleStruct));
  
    if (bytes_written > 0) sbuf_i = 0; /* invalidate buffer */
    
  }
}
