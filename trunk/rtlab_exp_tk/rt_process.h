/*
 * This file is part of the RT-Linux Multichannel Data Acquisition System
 *
 * Copyright (C) 1999,2000 David Christini
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

#ifndef _RT_PROCESS_H 
# define _RT_PROCESS_H

#include <version.h> /* in comedi's include/ or include/modbuild directory */
#if (! defined COMEDI_VERSION_CODE) 
#  define COMEDI_VERSION_CODE 0
#endif

#undef  OLD_STYLE_KCOMEDILIB
#undef  NEW_STYLE_KCOMEDILIB 

#if COMEDI_VERSION_CODE < 1856
#  define OLD_STYLE_KCOMEDILIB 1
#warning Support for old-style kcomedilib is deprecated.  Please upgrade to the latest comedi you can get your hands on!
#else
#  define NEW_STYLE_KCOMEDILIB 1
#endif 

#ifdef OLD_STYLE_KCOMEDILIB
typedef int COMEDI_T;
#else
typedef comedi_t* COMEDI_T;
#endif


# include <rtl_time.h>
# include "shared_stuff.h" /* for SampleStruct type */


# define BILLION (1000000000)
# define MILLION (1000000)

# define RT_PROCESS_MODULE_NAME "rt_process"

# define INITIAL_CHANNEL_GAIN 0  //initial channel gain (COMEDI)
# define INITIAL_SAMPLING_RATE_HZ 1000
# define INITIAL_SPIKE_BLANKING (INITIAL_SAMPLING_RATE_HZ/20) 
/* initial interval for disabling
   v_spike detect ... i.e. 1/20 sec, 50ms */


#define DEFAULT_COMEDI_DEVICE "/dev/comedi0"

typedef struct {
  char channel_mask[CHAN_MASK_SIZE]; /* mask of channels id's */
  hrtime_t acq_start; /* hrtime that data acquisition started for first chan.*/
  hrtime_t acq_end;   /* hrtime that data acquisition ended for last channel */
  unsigned int n_samples;            /* the number of valid elements in the 
                                        samples array */
  SampleStruct samples[SHD_MAX_CHANNELS];
} MultiSampleStruct;

typedef void (*rtfunction_t)(MultiSampleStruct *);

struct spike_info {
  /* The last time a spike was encountered in each channel */
  hrtime_t last_spike_time[SHD_MAX_CHANNELS];
  /* Same as SampleStruct.spike_period  */
  double period[SHD_MAX_CHANNELS]; /* in mlliseconds */
  /* the set of channels that have spikes */
  char spikes_this_scan[CHAN_MASK_SIZE];
};

/* 
   EXPORTED FUNCTIONS
   All of the below functions get exported into the kernel symbol table 
   Their purpose is to provide a dynamic mechanism for adding custom
   functionality to the rt_process.c module's main loop.

   For example, say you wanted to send analog output (in realtime) based on
   the present set of scans.  You would simply register a function
   that analyzes the MultiSamplesStruct passed into it and send comedi
   output to the ao_subdev in the SharedMemStruct, based upon the inputs.
*/

/* registers a function to be run within the rtf loop
   returns 0 on succes, ENOMEM or EBUSY on error.  
   Specified function won't be run (activated)
   until rtp_activate_function() is called 
   SIDE Effect: rt_process's use count is incremented */
extern int rtp_register_function(rtfunction_t function);
/* de-registers a function that was previously registered,
   returns 0 on success, EINVAL or EBUSY on error 
   SIDE Effect: rt_process's use count is decremented */
extern int rtp_unregister_function(rtfunction_t function);
/* de-activates a function that was previously registered,
   returns 0 on success, EINVAL on error */
extern int rtp_deactivate_function(rtfunction_t function);
/* activates a function that was previously registered,
   returns 0 on success, EINVAL on error */
extern int rtp_activate_function(rtfunction_t function);
/* Finds a free fifo, calls rtf_create(), and puts the minor number in minor.  
   On error a negative errno is returned. 
   Use this to quickly find a free fifos.  Helpful so that don't have to loop
   to find a free. */
extern int rtp_find_free_rtf(int *minor, int size);
/* 
   EXPORTED VARIABLES:
   Other variables present in rt_process.o: 
*/

/* The shared memory struct is also exported */
extern SharedMemStruct *rtp_shm;

/* the handle one should use when calling comedi_open() and comedi_close()
   the reason this handle is needed is because of the type/semantic
   changes between comedi 0.7.65+ and earlier versions */
extern COMEDI_T rtp_comedi_ai_dev_handle, rtp_comedi_ao_dev_handle;

/* Run-time spike information exported to the outside world. */
extern struct spike_info spike_info;

/* helper rounding function */
inline int round (double d) { return (int)(d + (d >= 0 ? 0.5 : -0.5)); }


#endif 
