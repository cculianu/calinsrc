/*
 * This file is part of the RT-Linux Multichannel Data Acquisition System
 *
 * Copyright (C) 2003 Calin Culianu
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
#ifndef _RTLAB_H
#define _RTLAB_H

#ifdef __cplusplus
extern "C" {
#endif

# define BILLION (1000000000)
# define MILLION (1000000)

# define RT_PROCESS_MODULE_NAME "rtlab"

# define INITIAL_CHANNEL_GAIN 0  /*initial channel gain (COMEDI)*/
# define INITIAL_SAMPLING_RATE_HZ 1000
# define THROTTLED_DOWN_SAMPLING_RATE_HZ 10 /* for when rtlab.o is idle */
# define MAX_SAMPLING_RATE_HZ 25000 //< Dangerous on some hardware!
# define MIN_SAMPLING_RATE_HZ 1 //< Will anyone _ever_ use this rate? 
# define DEFAULT_SETTLING_TIME_ns 0
/* initial interval for disabling
   v_spike detect ... i.e. 10ms */
# define INITIAL_SPIKE_BLANKING (10) 
# define INITIAL_SECONDS_VISIBLE (5)

# define DEFAULT_COMEDI_DEVICE "/dev/comedi0"

# define DEFAULT_LOG_FILE "default.log"
# define DEFAULT_NDS_FILE "data.nds"

/* The number of full scans (if all channels were turned on) to
   use in the AO and AI RT-Fifos -- defaults to 5 seconds worth 
   of scans! */
#define DEFAULT_FIFO_SECS 5
/** Each fifo shouldn't exceeed this size in bytes by default */
#define DEFAULT_MAX_FIFO 1000000

#define DEFAULT_SPIKE_BLANKING ((unsigned int)100) /* In milliseconds        */

#ifdef __cplusplus
}
#endif


#endif
