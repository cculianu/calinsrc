/*
 * This file is part of the RT-Linux Multichannel Data Acquisition System
 *
 * Copyright (C) 1999,2000 David Christini
 * Copyright (C) 2001,2002 Calin Culianu, David Christini
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

# include <linux/comedilib.h>

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


# include "rtos_middleman.h"
# include "shared_stuff.h" /* for SampleStruct type */

/** Some global defaults */
# include "rtlab_defaults.h"

typedef struct {
  char channel_mask[CHAN_MASK_SIZE]; /* mask of channels id's */
  hrtime_t acq_start; /* hrtime that data acquisition started for first chan.*/
  hrtime_t acq_end;   /* hrtime that data acquisition ended for last channel */
  unsigned int n_samples;            /* the number of valid elements in the 
                                        samples array */
  SampleStruct samples[SHD_MAX_CHANNELS];
} MultiSampleStruct;

/** For a given channel id and MultiSampleStruct, gives you the voltage
    for that channel.  If the channel is not on, returns 0. */
extern double voltage_at(unsigned int channel_id, const MultiSampleStruct *m);

typedef void (*rtfunction_t)(MultiSampleStruct *);

struct spike_info {
  /* The last time a spike was encountered in each channel */
  hrtime_t last_spike_time[SHD_MAX_CHANNELS]; /* when spike began */
  hrtime_t last_spike_ended_time[SHD_MAX_CHANNELS]; /* when spike ended */
  /* Same as SampleStruct.spike_period  */
  double period[SHD_MAX_CHANNELS]; /* in mlliseconds */
  /* the set of channels that have spikes - for THIS scan */
  char spikes_this_scan[CHAN_MASK_SIZE];

  char in_spike[CHAN_MASK_SIZE];       /* stateful bitmask indicating if
                                          we are in the middle of a spike */
  char saved_polarity[CHAN_MASK_SIZE]; /* if we are in_spike, this is relevant
                                          (and true means positive polarity) */
  double saved_thold[SHD_MAX_CHANNELS];  /* if we are in_spike, 
                                            this is relevant */
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

/* Next two functions: CALLBACK FREQUENCY RELATED FUNCTIONS */

/* rtp_set_callback_frequency --

   Sets how often the callback function is called.  

   Defaults is "called every scan" which is the same as passing 0 for 
   frequency_hz.  

      param frequency_hz: 
          The number of times per second this callback will be called.

          If you never set this, the defaults is the special value
          '0' which means 'call back every scan'.

   Note: frequency_hz _MUST_ be greater than or equal to the current sampling
   rate.  If it is not no error will be reported, but you will get undefined 
   behavior.
   
   This is because the callback mechanism relies on rtlab's sampling frequency.
   
   In other words, the rtlab realtime loop runs at a frequency that is the 
   current sampling rate, and the callbacks are issued from within that loop.

   For example -- If you are sampling at 1KHz the maximum allowable value
   for frequency_hz is really 1000, because we can't call the callbacks
   any faster than the realtime loop runs!

   Also, if frequency_hz is some strange value that is not evenly
   divisible by, or a multiple of, the sampling rate you will get undefined 
   behavior.    

   Return values: 0 on success, 
                  -EINVAL if funtion not found or
                  -EBUSY if module is busy.
*/
extern int rtp_set_callback_frequency(rtfunction_t function,uint frequency_hz);
/* 
   Returns the callback frequency, in Hz, for this function.  All
   functions default to the special value '0' which means
   "called every scan". 

   Return values: Positive value for frequency of callbacks in Hz on success, 
                  -EINVAL if funtion not found or
                  -EBUSY if module is busy.
*/
extern int rtp_get_callback_frequency(rtfunction_t function);

/* Finds a free fifo, calls rtf_create(), and puts the minor number in minor.  
   On error a negative errno is returned. 
   Use this to quickly find a free fifos.  Helpful so that don't have to loop
   to find a free. */

extern int rtp_find_free_rtf(int *minor, int size);


/* 
   Sets the sampling rate.  Returns the new (possibly normalized) rate. 
   Can be called from realtime context, but non realtime ok! 
*/
extern sampling_rate_t rtlab_set_sampling_rate(sampling_rate_t);

/* 
   EXPORTED VARIABLES:
   Other variables present in rt_process.o: 
*/

/* The shared memory struct is also exported */
extern SharedMemStruct *rtp_shm;

/* Data type for future use by rtlab rt_process API to capture a 
   device/subdev/channel/range/data combination in terms of a 'context' */
struct rtlab_comedi_context {
  COMEDI_T dev;
  unsigned int subdev;
  unsigned int chan;
  unsigned int range;
  unsigned int aref;
};

typedef struct rtlab_comedi_context rtlab_comedi_context;

#define EMPTY_CONTEXT ((rtlab_comedi_context){0,0,0,0,0})
#define CLEAR_CONTEXT(x) (memset(x, 0, sizeof(rtlab_comedi_context)))

/* the handle one should use when calling comedi_open() and comedi_close()
   the reason this handle is needed is because of the type/semantic
   changes between comedi 0.7.65+ and earlier versions */
extern COMEDI_T rtp_comedi_ai_dev_handle, rtp_comedi_ao_dev_handle;

/* Run-time spike information exported to the outside world. */
extern struct spike_info spike_info;

/* for round(), et al */
#include "kmath.h"

/* the proc dir root for rtlab -- in case additional modules want to add
   themselves to the proc fs */
struct proc_dir_entry;
extern struct proc_dir_entry *rtlab_proc_root;


/*-----------------------------------------------------------------------------
  UTILITY FUNCTIONS
-----------------------------------------------------------------------------*/

#include "kutil.h"

/* Helper functions to convert a sample to a volt for a given context,
   and vice-versa */   
extern double rtlab_lsampl_to_volts(const struct rtlab_comedi_context *,
                                   lsampl_t sampl);
extern lsampl_t rtlab_volts_to_lsampl(const struct rtlab_comedi_context *,
                                      double volts);
/* 
   Populates the context with an appropriate range setting that is capable
   of outputting and inputting voltage 'desired_voltage' 
   Returns 0 on success or EINVAL if not found 
*/
extern int rtlab_find_and_set_best_range(struct rtlab_comedi_context *, 
                                         double desired_voltage);

/* Convenience function for constructing an rtlab_comedi_context */
extern int rtlab_init_ctx(struct rtlab_comedi_context *,
                          unsigned int subdev_type,
                          unsigned int chan, 
                          double desired_voltage,
                          unsigned int aref);

#endif 
