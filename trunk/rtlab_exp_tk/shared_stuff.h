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
/*
  Shared Memory dot h 
  -------------------
  Defines all the shared parameters and structures  that are used for 
  kernel/userland communication  in the DAQ System
*/
#ifndef _SHARED_MEMORY_DOT_H
# define _SHARED_MEMORY_DOT_H

/** Some global defaults and constants */
#include "rtlab_defaults.h"

#ifdef __cplusplus
extern "C" {
#endif

  /* The size of one 'block' in the RTFs uses for sending samples to userland.
     One block is equal to sizeof(SampleStruct) -- never read anything smaller
     than this unit off the fifos! */
# define SS_RT_QUEUE_BLOCK_SZ_BYTES (sizeof(SampleStruct))

/* The maximum number of channels per subdevice for our automatically
   allocated shared memory struct */
# define SHD_MAX_CHANNELS 256

# define CHAN_MASK_SIZE (SHD_MAX_CHANNELS / 8)


#include "rtlab_types.h"

/* Spike Detection parameters.  This structure exists in the SharedMemStruct
   shared memory area and controls how the kernel module detects/works 
   with spikes... 
   
   ToDo: Separate this out into two separate modules.  rt_process itself
   should not care too much about spike detection.  :/
*/
#include "spike_polarity.h"


struct SpikeParams {
  volatile int8 enabled_mask [CHAN_MASK_SIZE];       /* bits correspond to channels.. */
  volatile int8 polarity_mask [CHAN_MASK_SIZE];      /* if bit set: positive          */
  volatile unsigned int blanking [SHD_MAX_CHANNELS]; /* in milliseconds               */
  volatile double threshold [SHD_MAX_CHANNELS];      /* NaNs here can be dangerous!   */
};

#ifndef __cplusplus
typedef struct SpikeParams SpikeParams;
#endif

/*
  SharedMemStruct
  ---------------
  This structure is what gets put into the shared memory segment
  for configuration-related communication from the non-real-time
  process to the real-time task.
*/
# define SHARED_MEM_NAME "DAQ System SHM" /* text ID for mbuff      */
# define SHD_SHM_STRUCT_VERSION 66        /* test this against the below 
                                             struct_version member           */
struct SharedMemStruct {
#ifdef __cplusplus
/* prevent compiler errors due to consts below */
  SharedMemStruct(): struct_version(SHD_SHM_STRUCT_VERSION)  {}
#endif
  int struct_version;                    /* magic number used to make sure
                                            this structure version is synch'd
                                            between userland and kernel land 
                                            kernel sets this to 
                                            SHD_SHM_STRUCT_VERSION and
                                            userland tests against that
                                            define                           */
  
  /* read/write struct memebers for kernel and userland                      */
  volatile  unsigned int ai_chan[SHD_MAX_CHANNELS]; /* comedi channel structure
						       for analog channel 
						       parameters, use CR_PACK 
						       et al to modify this  */
  volatile  unsigned int ao_chan[SHD_MAX_CHANNELS]; /* (currently unused)    */
  
  volatile  int8 ai_chans_in_use[CHAN_MASK_SIZE]; /* Bitwise mask to tell the 
						     kernel  process which 
						     channels to ignore. 
                                                     Use inline funcs 
						     set_chan() and 
                                                     is_chan_on() to set this*/
  volatile  int8 ao_chans_in_use[CHAN_MASK_SIZE];  
  
  /* The constant sampling rate --
     this directly affects the period of the rt loop.  
     Initialized at rtlab.o module insertion time to a configurable 
     default of 1000  */
  sampling_rate_t sampling_rate_hz; 
  
  volatile  scan_index_t scan_index; /* index of current analog input scan --  
                                        notice how this property is writable..
                                        we can reset this at any time from the 
                                        user process so as to allow the ability
                                        to reset the scan index              */
  volatile  unsigned int nanos_per_scan; /* Basically the number of 
                                            nanoseconds  per scan.  This
                                            is a trivial function
                                            of BILLION / sampling rate       */
  
  SpikeParams spike_params;              /* see struct definition above      */
  int    attached_pid;  /* Normally the PID of daq_system, but crashed
                           daq_system's can forget to clean up after themselves
                           here! */
  /* read-only struct members for userland, read/write for kernel            */
  int ai_minor;                   /* /dev/comediX                     */
  int ao_minor;                   /* /dev/comediX (currently unused)  */
  int ai_subdev;                  /* the comedi subdevice index fr ai */
  int ao_subdev;                  /* comedi ao subdevice index        */
  int ai_fifo_minor;              /* the /dev/rtfX where ai sampls go */
  int ao_fifo_minor;              /* (currently unused)               */
  int control_fifo;               /* The RT-FIFO rtlab.o listens on
                                     for control commands..
                                     (unimplemented) */
  int reply_fifo;                 /* Reply FIFO from rtlab.o to user  */
  unsigned int n_ai_chans;        /* the number of channels in subdev */
  unsigned int n_ao_chans;        /* ditto                            */
  volatile  unsigned int jitter_ns;/* Jitter of rt-task in nanos      */
  unsigned int ai_fifo_sz_blocks; /* The size of the RT-Queue in terms
                                            of SS_RT_QUEUE_BLOCK_SZ_BYTES 
                                            units */
  unsigned int ao_fifo_sz_blocks; /* The size of the RT-Queue in terms
				     of SS_RT_QUEUE_BLOCK_SZ_BYTES 
				     units */

  /* Keep track of real wall clock time as scan_index is not monotonically
     increasing (due to the fact that sampling rate can change)       */
  uint64 time_ms;                  /* The current relative time in ms. 
				      This is the monotonically increasing  
				      number of milliseconds since rtlab.o
				      was loaded. */
};
#ifndef __cplusplus
typedef struct SharedMemStruct SharedMemStruct;
#endif

static inline 
int
_test_bit (unsigned int bit, volatile const char *bits)
{
  unsigned int elem_index = bit / 8, bitpos = bit % 8;
  char mask = 1;

  mask <<= bitpos;

  return (bits[elem_index] & mask) != 0;
}

static inline
void
_set_bit (unsigned int bit, volatile char *bits, int onoroff)
{
  unsigned int elem_index = bit / 8, bitpos = bit % 8;
  char mask = 1;

  mask <<= bitpos;

  if (onoroff)
    bits[elem_index] |= mask;
  else 
    bits[elem_index] &= ~mask;    
}

static inline 
int
is_chan_on (unsigned int chan, volatile const char array[CHAN_MASK_SIZE])
{ return (chan > SHD_MAX_CHANNELS ? 0 : _test_bit(chan, array) ); }

static inline 
void
set_chan (unsigned int chan, volatile char array[CHAN_MASK_SIZE], int onoroff)
{ if (chan < SHD_MAX_CHANNELS) _set_bit(chan, array, onoroff); }

static inline 
void
init_spike_params (SpikeParams *p)
{
  register unsigned int i;

  for (i = 0; i < CHAN_MASK_SIZE; i++) {
    p->enabled_mask[i] = 0;  
    p->polarity_mask[i] = 0;
  }

  for (i = 0; i < SHD_MAX_CHANNELS; i++) {
    p->blanking[i] = DEFAULT_SPIKE_BLANKING;
    p->threshold[i] = 0.0;
  }
}

/* For round, et al */
#include "kmath.h"

/** This enforces sampling rates to be 'millisecond friendly' numbers  --
    numbers that are factors of 1000 or are multiples of 1000 */
static inline sampling_rate_t normalizeSamplingRate(sampling_rate_t rate)
{ 
  sampling_rate_t ret = INITIAL_SAMPLING_RATE_HZ;
  int multiple = 0;

  /** Make sure it's within bounds... */
  if (rate > MAX_SAMPLING_RATE_HZ ) ret = MAX_SAMPLING_RATE_HZ;
  else if (rate < MIN_SAMPLING_RATE_HZ ) ret = MIN_SAMPLING_RATE_HZ;

  if (rate > 1000 && (multiple = round(rate / 1000.0))) { /* rate > 1000 */
    ret = multiple * 1000;
  } else if (rate && (multiple = round(1000.0 / rate)) ) { /* rate < 1000 */
    ret = 1000 / multiple;
  }

  return ret;
}


/* 
   The SampleStruct structure
   -------------------------
   This structure encapsulates one particular data sample.

   For example:
   It it put into the /dev/rtf? special device file FIFO's by rt_proccess.c.
   The fomat for that is basically the special FIFO_DELIMITER byte followed by
   1 of the below structs, and so on.


   channel_id - the channel that this sample corresponds to
   scan_index - this counter keeps going up as rt_process runs, +1 for each
                scan it does
   data -       an element of lsample_t which contains the sample obtained 
                from one particular channel at one particular time.
   magic_number - sanity check on this should always be equal to
                  SAMPLE_STRUCT_MAGIC
*/
#define SAMPLE_STRUCT_MAGIC ((int)0xbeeff00d)
struct SampleStruct {  
#ifdef __cplusplus
  SampleStruct() : magic_number(SAMPLE_STRUCT_MAGIC) {};
#endif
  uint8 channel_id;
  scan_index_t scan_index;
  double data;         /* the actual sample, in volts */
  uint8 spike; /* if nonzero, there is a spike this scan */
  double spike_period; /* in milliseconds */
  int magic_number;    /*if this is wrong userland can throw out this 'sample'
                         this might not be necessary, since kernel will always 
                         do complete writes and userland should
                         be doing reads in sizeof(SampleStruct) chunks..
                         however a crashed (or maybe even pre-empted?)
                         user process can throw the whole thing off 
                         theoretically, and lead to bad data being accepted
                         (ergo the delimiter bytes [see above macro] 
                         is needed to re-align the stream...) :/
                         This magic number also is a sort of version check
                       */
  
}; 

#ifndef __cplusplus
typedef struct SampleStruct SampleStruct;
#endif

#ifdef __cplusplus
inline bool operator==(const SampleStruct &s1, const SampleStruct &s2)
{
  return (s1.channel_id == s2.channel_id && s1.scan_index == s2.scan_index && s1.data == s2.data);
}
#endif

#ifdef __cplusplus
}
#endif

#endif
