/*
 * This file is part of the RT-Linux Multichannel Data Acquisition System
 *
 * Copyright (C) 2002 Calin Culianu
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
#include <linux/module.h> 
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/string.h>

#include "rtlab.h"

#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL"); 
#endif

#define MODULE_NAME "Example RTLab Module"

MODULE_AUTHOR("David J. Christini, PhD and Calin A. Culianu");
MODULE_DESCRIPTION(MODULE_NAME ": A Real-Time stimulation and control "
                   "add-on for daq system and rtlab.o.\n$Id$");

int  mod_init(void);      /* Module entry */
void mod_cleanup(void);   /* Module cleanup */

module_init(mod_init);    /* as per Linux Kernel Module interface */
module_exit(mod_cleanup); /* as per Linux Kernel Module interface */

#define N_VOLTAGES 10 /* the number of voltages to average */
static double voltage_avg = 0.0;
static int n_voltages = 0; /* the number of voltages in our average */
static double frequency_hz = 0.0; /* our ongoing frequency variable F */
static struct rtlab_monitor *monitor = 0; /* the opaque handle given to us by 
                                             rtlab.o */

/* just a struct holding our string message which gets copied to 
   user space via a realtime fifo */
struct fifo_msg {
  char message[1024];
};

#define FIFO_SZ (sizeof(struct fifo_msg) * 100)
static int realtime_fifo = -1; /* handle to our outgoing RTF */
static int ao_chan = -1; /* the analog output channel handle reserved
                            for us by rtlab.o */

/* this is called once per scan to recompute voltage_avg */
static void recompute_voltage_average(MultiSampleStruct *);

/* this is called automatically by the signal monitor as a notification
   that the frequency of the monitored channel changed, or that one 
   period elapsed in the monitored channel.   */
static void monitored_values_changed(void);

/* internally called by monitored_values_changed() to send a stimulus
   down the wire using rtlab_stimulate_ao_chan() */
static void stimulate(void); /* if called, a new stim is started */

/* internal function that writes a struct fifo_msg out the rt-fifo
   so that a monitoring user process can serialize experiment data to disk */
static void out_to_fifo(void);


int mod_init (void)
{
  int retval = 0;

  if (
      /* register our per-scan callback */
      (retval = rtp_register_function(recompute_voltage_average))
      /* find a non-reserved rtf (realtime fifo) and put its value
         in realtime_fifo */
       || (retval = rtp_find_free_rtf(&realtime_fifo, FIFO_SZ))
      /* find a non-reserved analog output channel for control
         and put its value in ao_chan */
       || (retval = rtp_reserve_ao_chan(&ao_chan)) 
       || (retval = rtlab_monitor_channel(
                                          /* opaque 'handle' */
                                          &monitor, 

                                          /* monitor the first AI chan 
                                             on the first comedi  board */
                                          0,  

                                          /* tell the monitor we are
                                             interested in frequency only */
                                          RTLAB_MONITOR_FREQUENCY, 

                                          /* our notification callback
                                             function */
                                          monitored_values_changed))

      /* activate our callback function once all of the above is ok */
       || (retval = rtp_activate_function(recompute_voltage_average))
     ) 
    /* on failure call our module cleanup function to release
       any partial resources */
    mod_cleanup();  
 
  return retval;
}

void mod_cleanup (void)
{
  rtp_deactivate_function(recompute_voltage_average);

  /* free our channel monitor */
  if (monitor)  rtlab_deactivate_monitor(monitor);

  /* free up our reserved rt-fifo */
  if (realtime_fifo >= 0)  rtp_rtf_destroy(realtime_fifo);

  /* indicate that now this channel is free */
  if (ao_chan >= 0)  rtp_free_ao_channel(ao_chan);  

  rtp_unregister_function(recompute_voltage_average);
}

void recompute_voltage_average(MultiSampleStruct *)
{
  if (n_voltages < N_VOLTAGES) 
    n_voltages++;

  
  avg_voltage +=  m->samples[0] / n_voltages;
}

void monitored_values_changed(void)
{
  /* abort early if our voltage_avg value isn't yet complete 
     (recomput_voltage_average needs to be called N_VOLTAGES times
     in order for our voltage_avg value to be considered value) */
  if (n_voltages < N_VOLTAGES) return;

  frequency_hz = rtlab_get_frequency(monitor);
  
  stimulate();
  out_to_fifo();
}

void stimulate(void)
{
  struct rtlab_stim_params rsp = EMPTY_STIM_PARAMS;

  /* stimulate with an amplitude that is 1/10th the ongoing computed
     average voltage */
  rsp.on_voltage = voltage_avg / 10.0;  
  rsp.off_voltage = 0;
  rsp.when_ms = 0; /* send the stim right now (0 ms from now) */
  /* the duration in ms is one tenth the current computed frequency */
  rsp.duration_ms = (1000.0 / frequency_hz) / 10.0; 
  /* only 1 stim per 'stim train' */
  rsp.num_per_train = 0;
  /* only 1 stim cycle */
  rsp.num_trains = 1;

  rtlab_stimulate_ao_chan(ao_chan, &rsp);
}

static void out_to_fifo(void)
{
  struct fifo_msg fm;

  sprintf(fm.message,
          "Scan index: %s   Avg: %s V    Freq: %s HZ", 
          rtlab_scan_index_str, 
          rtlab_double_to_string(voltage_avg),
          rtlab_double_to_string(frequency_hz));
  
  rtp_rtf_put(realtime_fifo, &fm, sizeof(fm));
}
