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
#  include <linux/comedilib.h>
# else
#  define dconst const /* dconst is const in userland (enforce read-only) */
#  include <comedi.h>
# endif

#ifdef __cplusplus
extern "C" {
#endif

/* The maximum number of channels per subdevice for our automatically
   allocated shared memory struct */
# define SHD_MAX_CHANNELS 256

# define CHAN_MASK_SIZE SHD_MAX_CHANNELS / 8


/* Used in SharedMemStruct and SignalParams                                  */
typedef unsigned long long scan_index_t; 


/*
  SharedMemStruct
  ---------------
  This structure is what gets put into the shared memory segment
  for configuration-related communication from the non-real-time
  process to the real-time task.
*/
# define SHARED_MEM_NAME "RT-Linux DAQ System SHM" /* text ID for mbuff      */
# define SHD_SHM_STRUCT_VERSION 055      /* test this against the below 
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
  unsigned int sampling_rate_hz; /* Use this to modify the period of the rt
				    loop (dangerous!).  Initialized to 1000  */

  /* read-only struct members for userland, read/write for kernel            */
  dconst int ai_minor;                   /* /dev/comediX                     */
  dconst int ao_minor;                   /* /dev/comediX (currently unused)  */
  dconst int ai_subdev;                  /* the comedi subdevice index fr ai */
  dconst int ao_subdev;                  /* comedi ao subdevice index        */
  dconst int ai_fifo_minor;              /* the /dev/rtfX where ai sampls go */
  dconst int ao_fifo_minor;              /* (currently unused)               */
  dconst unsigned int n_ai_chans;        /* the number of channels in subdev */
  dconst unsigned int n_ao_chans;        /* ditto                            */
  dconst scan_index_t scan_index; /* index of current analog input scan      */
};
typedef struct SharedMemStruct SharedMemStruct;


/* FIFO_DELIMITER is put into the rtf right before each SampleStruct and can 
   act as a sort of alignment measure. 
   TODO: Discover if 8 'ff' bytes can possibly ever occur inside 
   SampleStruct.  My gut feeling is that no, they can't 
   (unless rt_process has been running for thousands of years and the 
   board it runs on supports 32-bit samples with 256 channels.) */
//#define FIFO_DELIMITER_T long long
//#define FIFO_DELIMITER ((FIFO_DELIMITER_T)0xffffffffffffffff)
//#define DECLARE_FIFO_DELIMITER(var) FIFO_DELIMITER_T var = FIFO_DELIMITER

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

typedef struct {  
  //#ifdef __cplusplus
  //SampleStruct () : channel_id(0), scan_index(0), range(0), data(0), 
  //magic_number(SAMPLE_STRUCT_MAGIC) {};
  //#endif
  unsigned char channel_id;
  scan_index_t scan_index;
  unsigned int range; /* the range setting at which this sample was recorded */
  lsampl_t data;  /* the actual sample */
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
  
} SampleStruct; 


inline 
int
is_chan_on (unsigned int chan, char array[CHAN_MASK_SIZE])
{
  int elem_index = chan / 8, bitpos = chan % 8, i;
  char mask = 1;

  if (chan > SHD_MAX_CHANNELS) return 0;
  for (i = 0; i < bitpos; i++) mask *= 2;
  return (array[elem_index] & mask);
}

inline
void
set_chan (unsigned int chan, char array[CHAN_MASK_SIZE], int onoroff)
{
  unsigned int elem_index = chan / 8, bitpos = chan % 8, i;
  char mask = 1;

  for (i = 0; i < bitpos; i++) mask *= 2;
  if (onoroff)
    array[elem_index] |= mask;
  else 
    array[elem_index] &= ~mask;
}

# undef dconst

#ifdef __cplusplus
}
#endif

#endif
