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

# include <mbuff.h> 
# include <rtl_time.h>
# include "shared_stuff.h" /* for SampleStruct type */


# define BILLION ((long int)1000000000)
# define MILLION (1000000)

# define RT_PROCESS_MODULE_NAME "rt_process"

# define INITIAL_CHANNEL_GAIN 0  //initial channel gain (COMEDI)
# define INITIAL_V_CHANNEL 0     //initial channel for V signal
# define INITIAL_A_CHANNEL 1     //initial channel for A signal
# define INITIAL_SAMPLING_RATE_HZ 1000
# define INITIAL_SPIKE_BLANKING INITIAL_SAMPLING_RATE_HZ/20 
/* initial interval for disabling
   v_spike detect ... i.e. 1/20 sec, 50ms */


# define DEFAULT_COMEDI_MINOR   0    /* /dev/comediX device to use: this 
                                        should some day come from a config 
                                        script?                              */

# define DEFAULT_AI_FIFO        0    /* the /dev/rtfX device to use: this 
                                        should some day come from a config 
                                        script and/or be more dynamic?       */
# define DEFAULT_AO_FIFO        1    /* the /dev/rtfX device to use: this 
                                        should some day come from a config 
                                        script and/or be more dynamic?       */

# define DEFAULT_SPIKE_FIFO     2    /* the /dev/rtfX device to use: this
                                        should some day be more dynamic, 
                                        such as coming from a config
                                        script.                              */
typedef struct {
  char channel_mask[CHAN_MASK_SIZE]; /* mask of channels id's */
  hrtime_t acq_start; /* hrtime that data acquisition started for first chan.*/
  hrtime_t acq_end;   /* hrtime that data acquisition ended for last channel */
  unsigned int n_samples;            /* the number of valid elements in the 
                                        samples array */
  SampleStruct samples[SHD_MAX_CHANNELS];
} MultiSampleStruct;

typedef void (*rtfunction_t)(SharedMemStruct *, MultiSampleStruct *);

/* functions to be run at the end of the real-time daq loop
   they are chained on, one after the other */
struct rt_function_list {
  char active_flag;
  rtfunction_t function;
  struct rt_function_list *next;
};

struct spike_info {
  /* The last time a spike was encountered in each channel */
  hrtime_t last_spike_time[SHD_MAX_CHANNELS];
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
int rtp_register_function(rtfunction_t function);
/* de-registers a function that was previously registered,
   returns 0 on success, EINVAL or EBUSY on error 
   SIDE Effect: rt_process's use count is decremented */
int rtp_unregister_function(rtfunction_t function);
/* de-activates a function that was previously registered,
   returns 0 on success, EINVAL on error */
int rtp_deactivate_function(rtfunction_t function);
/* activates a function that was previously registered,
   returns 0 on success, EINVAL on error */
int rtp_activate_function(rtfunction_t function);




/* 
   EXPORTED VARIABLES:
   Other variables present in rt_process.o: 
*/

/* Run-time spike information exported to the outside world. */
extern struct spike_info spike_info;

#endif 
