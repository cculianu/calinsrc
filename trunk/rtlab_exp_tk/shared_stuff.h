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


/* dconst, or dummy const is a hackish way to enforce the fact that userland
   should NOT modify certain key shared memory struct members */
# ifdef MODULE
#  define dconst /* */ /* dconst is nothing in kernel land.. so we can write */
# else
#  define dconst const /* dconst is const in userland (enforce read-only) */
# endif

#ifdef __cplusplus
extern "C" {
#endif


# define RT_QUEUE_SZ_BLOCKS (24000) /* size of queue in terms of \
                                       number of sample blocks */
# define RT_QUEUE_BLOCK_SZ_BYTES (sizeof(SampleStruct))
/* size of RT queue (RTF's) in bytes. */
# define RT_QUEUE_SZ_BYTES (RT_QUEUE_SZ_BLOCKS * RT_QUEUE_BLOCK_SZ_BYTES) 

/* The maximum number of channels per subdevice for our automatically
   allocated shared memory struct */
# define SHD_MAX_CHANNELS 256

# define CHAN_MASK_SIZE (SHD_MAX_CHANNELS / 8)


/* Used in SharedMemStruct                                                   */
typedef unsigned long long scan_index_t;
typedef unsigned int sampling_rate_t;


/* Spike Detection parameters.  This structure exists in the SharedMemStruct
   shared memory area and controls how the kernel module detects/works 
   with spikes... 
   
   ToDo: Separate this out into two separate modules.  rt_process itself
   should not care too much about spike detection.  :/
*/
#include "spike_polarity.h"

#define DEFAULT_SPIKE_BLANKING ((unsigned int)100) /* In milliseconds        */

struct SpikeParams {
  char enabled_mask [CHAN_MASK_SIZE];       /* bits correspond to channels.. */
  char polarity_mask [CHAN_MASK_SIZE];      /* if bit set: positive          */
  unsigned int blanking [SHD_MAX_CHANNELS]; /* in milliseconds               */
  double threshold [SHD_MAX_CHANNELS];      /* NaNs here can be dangerous!   */
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
# define SHARED_MEM_NAME "RT-Linux DAQ System SHM" /* text ID for mbuff      */
# define SHD_SHM_STRUCT_VERSION 056      /* test this against the below 
                                            struct_version member            */
struct SharedMemStruct {
#ifdef __cplusplus
  /* prevent compiler errors due to consts below */
  SharedMemStruct(): struct_version(SHD_SHM_STRUCT_VERSION) {};
#endif
  dconst int struct_version;             /* magic number used to make sure
                                            this structure version is synch'd
                                            between userland and kernel land 
                                            kernel sets this to 
                                            SHD_SHM_STRUCT_VERSION and
                                            userland tests against that
                                            define                           */

  /* read/write struct memebers for kernel and userland                      */
  unsigned int ai_chan[SHD_MAX_CHANNELS]; /* comedi channel structure for 
                                             analog channel parameters, use
                                             CR_PACK et al to modify this    */
  unsigned int ao_chan[SHD_MAX_CHANNELS]; /* (currently unused)              */
                           
  char ai_chans_in_use[CHAN_MASK_SIZE]; /* Bitwise mask to tell the kernel 
                                           process which channels to ignore. 
                                           Use inline funcs set_chan() and 
                                           is_chan_on() to set this           */
  char ao_chans_in_use[CHAN_MASK_SIZE];  
  sampling_rate_t sampling_rate_hz; /* Use this to modify the period of the rt
                                       loop (dangerous!).  Initialized to 1000  */
  
  scan_index_t scan_index; /*       index of current analog input scan --   
                                    notice how this property is writable..
                                    we can reset this at any time from the 
                                    user process so as to allow the ability
                                    to reset the scan index                  */
				     
  SpikeParams spike_params;              /* see struct definition above      */

  /* read-only struct members for userland, read/write for kernel            */
  dconst int ai_minor;                   /* /dev/comediX                     */
  dconst int ao_minor;                   /* /dev/comediX (currently unused)  */
  dconst int ai_subdev;                  /* the comedi subdevice index fr ai */
  dconst int ao_subdev;                  /* comedi ao subdevice index        */
  dconst int ai_fifo_minor;              /* the /dev/rtfX where ai sampls go */
  dconst int ao_fifo_minor;              /* (currently unused)               */
  dconst int spike_fifo_minor;           /* /dev/rtfX where spike samples go */
  dconst unsigned int n_ai_chans;        /* the number of channels in subdev */
  dconst unsigned int n_ao_chans;        /* ditto                            */
};
#ifndef __cplusplus
typedef struct SharedMemStruct SharedMemStruct;
#endif

inline 
unsigned int
_test_bit (unsigned int bit, const char *bits)
{
  unsigned int elem_index = bit / 8, bitpos = bit % 8;
  char mask = 1;

  mask <<= bitpos;

  return (bits[elem_index] & mask) != 0;
}

inline
void
_set_bit (unsigned int bit, char *bits, int onoroff)
{
  unsigned int elem_index = bit / 8, bitpos = bit % 8;
  char mask = 1;

  mask <<= bitpos;

  if (onoroff)
    bits[elem_index] |= mask;
  else 
    bits[elem_index] &= ~mask;    
}

inline 
unsigned int
is_chan_on (unsigned int chan, const char array[CHAN_MASK_SIZE])
{ return (chan > SHD_MAX_CHANNELS ? 0 : _test_bit(chan, array) ); }

inline
void
set_chan (unsigned int chan, char array[CHAN_MASK_SIZE], int onoroff)
{ if (chan < SHD_MAX_CHANNELS) _set_bit(chan, array, onoroff); }

inline
void
init_spike_params (SpikeParams *p)
{
  register uint i;

  for (i = 0; i < CHAN_MASK_SIZE; i++) {
    p->enabled_mask[i] = 0;  
    p->polarity_mask[i] = 0;
  }

  for (i = 0; i < SHD_MAX_CHANNELS; i++) {
    p->blanking[i] = DEFAULT_SPIKE_BLANKING;
    p->threshold[i] = 0.0;
  }
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
   magic_number - sanity check on this struct should always be equal to
                  SAMPLE_STRUCT_MAGIC
*/
//#define SAMPLE_STRUCT_MAGIC 0xbeeff00d
struct SampleStruct {  
  unsigned char channel_id;
  scan_index_t scan_index;
  double data;        /* the actual sample, in volts */
  /*int magic_number; if this is wrong userland can throw out this 'sample'
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
# undef dconst

#ifdef __cplusplus
}
#endif

#endif
