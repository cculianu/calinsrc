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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/errno.h>

#include <rtl.h>
#include <rtl_fifo.h>
#include <rtl_sched.h>
#include <rtl_sync.h>
#include <rtl_debug.h>

#include <comedi.h>           /* main comedi header file */

#include "rt_process.h"       /* header file specific to this program */

int init_module(void);        /* set up RT stuff */
void cleanup_module(void);    /* clean up RT stuff */
void init_shared_mem(void);   /* set up RT shared memory */

pthread_t daq_task;        /* main RT task */
volatile SharedMemStruct *sh_mem; /* define mbuff shared memory */

comedi_trig ai_trig;          /* main comedi trigger structure */
static FIFO_struct ai_fifo_struct; /* holds data array scanned from DAQ card */
                                   /* all channels scanned at once then placed */
                                   /* onto one FIFO in one rtf_put */
int channel_index,
  ioctl_check=0,         /* flag variable to check comedi_trig_ioctl success */
  ret;

static const unsigned int
       getFifos[NUM_GET_FIFOS] = GET_FIFOS, /* Our set of get fifos*/
       putFifos[NUM_PUT_FIFOS] = PUT_FIFOS; /* Our set of put fifos*/

#define NUM_TIME_CHECK 10                /* num times through loop to check */
long time_tick_check[2][NUM_TIME_CHECK]; /* check time first 10 times through */

void *daq(void *arg) { /* this task reads and writes data to a DAQ board */
  int i; /*  loop index */

  while (1) { /* must loop, b/c it is a periodic task */
      
      sh_mem->scan_index++; /* increment scan_index counter */

      /* Analog Input ... all channels in one ioctl call */

      /* gain of ai_trig is changed via sh_mem->ai_chan in non-rt-process */
      /*   __comedi_trig_ioctl instead of comedi_trig_ioctl b/c  */
      /*   it doesn't have as much overhead and is therefore faster */
      /*   doesn't initialize the board every time */

      ioctl_check = __comedi_trig_ioctl(AI_DEV,AI_SUBDEV,&ai_trig);
      
      /* Place the ai_fifo_struct data into each of the 'Get' fifos
         so that our Non-RT process(es) can pick them up */
      for (i = 0; i < NUM_GET_FIFOS; i++) {
	/* Note: error checking is not done here.  The fifo must meet
	   the following three conditions:
	   1) Tt must have been created with a call to rtf_create
	   2) It must be a fifo that is less than RTF_NO
	   3) It must have sufficient space for ai_fifo_struct
	   Otherwise, this put will fail! */
	rtf_put(getFifos[i], &ai_fifo_struct, sizeof(ai_fifo_struct));
      }

      /* End of Analog Input */

      ret=SpikeDetect();

      pthread_wait_np();             /* wait until beginning of next period */

    }
  return (void *)0;
}

int init_module(void) {
  unsigned int getIndex, putIndex;      /*  for initializing FIFOs */
  pthread_attr_t attr;
  struct sched_param sched_param;
  const char *errorMessage = "Success";

  if ( comedi_lock_ioctl(AI_DEV,AI_SUBDEV)    /* lock subdevice  */
       || comedi_lock_ioctl(AO_DEV,AO_SUBDEV) /* lock subdevice  */
       ) {
    errorMessage = "Error locking subdevice on comedi_lock_ioctl";
    goto init_error;
  }
     
 
  init_shared_mem();                        /* initialize shared memory */

  /* set up trigger structure for ai ... see comedi documentation for specifics */
  ai_trig.subdev = AI_SUBDEV;
  ai_trig.mode = 0;
  ai_trig.flags = 0;
  ai_trig.n_chan = sh_mem->num_ai_chan_in_use;  /* # chans to scan each ioctl() */
  ai_trig.chanlist = (int *)sh_mem->ai_chan; /* use sh_mem so non RT process can  */
                                        /* change channel gain via CR_PACK */
  ai_trig.data=ai_fifo_struct.data;     /* array scans are placed into */
  ai_trig.n=1;
  ai_trig.data_len = ai_trig.n_chan * ai_trig.n * sizeof(sampl_t);
  ai_trig.trigsrc=0;
  ai_trig.trigvar=0;
  ai_trig.trigvar1=0;

  /*  creates rt FIFOs as defined in rt_process.h */
  for (getIndex = 0, putIndex = 0; 
       getIndex < NUM_GET_FIFOS || putIndex < NUM_PUT_FIFOS ; 
       getIndex++, putIndex++) {

    if ( (getIndex < NUM_GET_FIFOS) 
	&& rtf_create(getFifos[getIndex], RT_QUEUE_SIZE)
       ) {
      errorMessage = "Cannot create a get fifo!";
      goto init_error;
    }
    if ( (putIndex < NUM_PUT_FIFOS) 
	 && rtf_create(putFifos[putIndex], RT_QUEUE_SIZE)
       ) { 
      errorMessage = "Cannot create a put fifo!";
      goto init_error;
    }
  }

  /* create the realtime task */
  pthread_attr_init(&attr);
  sched_param.sched_priority = 1;
  pthread_attr_setschedparam(&attr, &sched_param);
  if ( pthread_create(&daq_task, &attr, daq, (void *)0) ) {
    errorMessage = "Cannot create the daq pthread";
    goto init_error;
  }

  /* make task periodic: run at a rate of RT_HZ */
  if (pthread_make_periodic_np(daq_task, gethrtime(), NSECS_PER_SEC/RT_HZ)) {
    errorMessage = "Cannot make daq thread periodic";
    goto init_error;
  }
  rtl_printf(RT_PROCESS_ID ": acquisition started at %d Hz (%s)\n", 
	 RT_HZ, errorMessage);
  return 0;

 init_error:
  rtl_printf (RT_PROCESS_ID ": Failure -- can't initalize RT task (%s)\n", 
	      errorMessage);
  return 1;
}

void init_shared_mem(void) { /*  Initialize the rtlinux shared memory */

  int i;

  /* Open mbuff Shared Memory structure */
  sh_mem = 
    (volatile SharedMemStruct*) mbuff_alloc (RT_PROCESS_ID,
					     sizeof(SharedMemStruct));

  /* initialize shared memory variables ... see rt_process.h for definitions */
  sh_mem->scan_index = 0;
  sh_mem->num_ai_chan_in_use = NUM_AD_CHANNELS_TO_USE; /* num AI channels in use */

  for (i=0; i < sh_mem->num_ai_chan_in_use ; i++) { /* set channel parameters */
    sh_mem->signal_params[i].spike_blanking = InitSpikeBlanking;
    sh_mem->signal_params[i].spike_threshold = SCAN_UNITS;
    sh_mem->signal_params[i].spike_index = 0;
    sh_mem->signal_params[i].spike_polarity = 1;/* TRUE; */
    sh_mem->signal_params[i].found_spike = 0;   /* not yet found a spike */
    sh_mem->signal_params[i].previous_spike = 0;/* not yet found a spike */
    sh_mem->signal_params[i].since_prev_spike = InitSpikeBlanking;/* for 1st spike */
    sh_mem->signal_params[i].num_over_threshold = 0;

    sh_mem->ai_chan[i] = CR_PACK(i,INITIAL_CHANNEL_GAIN,AREF_GROUND);
  }
}

void cleanup_module(void) {
  int ret;
  unsigned int getIndex, putIndex;

  /*  destroys rt FIFOs as defined in rt_process.h */
  for (getIndex = 0, putIndex = 0; 
       getIndex < NUM_GET_FIFOS || putIndex < NUM_PUT_FIFOS ; 
       getIndex++, putIndex++) {
    if (getIndex < NUM_GET_FIFOS) {
      rtf_destroy(getFifos[getIndex]);
    }
    if (putIndex < NUM_GET_FIFOS) {
      rtf_destroy(putFifos[putIndex]);
    }
  }


  /*  cancel any pending ai operation */
  ret = comedi_cancel_ioctl(AI_DEV,AI_SUBDEV);

  /* unlock subdevice so other comedi programs can use */
  ret=comedi_unlock_ioctl(AI_DEV,AI_SUBDEV);

  /* unlock subdevice so other comedi programs can use */
  ret=comedi_unlock_ioctl(AO_DEV,AO_SUBDEV);

  /* delete daq_task */
  ret= pthread_delete_np(daq_task);
  rtl_printf(RT_PROCESS_ID ": deleted RT task (status %d) after %d cycles (~%d seconds).\n",
	 ret, sh_mem->scan_index, sh_mem->scan_index/RT_HZ);

  /* free up shared memory */
  mbuff_free(RT_PROCESS_ID,(void *)sh_mem);

}

int SpikeDetect(void) {
  /* check for spike in each AI signal: */
  /* must have been at least spike_blanking scans since last v_spike */
  /* must be at least NumOverThreshold scans in a row over threshold */
  /* (prevents noise from being detected) */

  int jj;
 
  for (jj=0; jj<sh_mem->num_ai_chan_in_use; jj++) {
  
    (sh_mem->signal_params[jj].since_prev_spike)++;
    if ( ( (sh_mem->signal_params[jj].since_prev_spike) >= 
	   sh_mem->signal_params[jj].spike_blanking) && 
	 ( ((ai_fifo_struct.data[jj]>
	     sh_mem->signal_params[jj].spike_threshold) && 
	    (sh_mem->signal_params[jj].spike_polarity)) ||
	   ((ai_fifo_struct.data[jj]<
	     sh_mem->signal_params[jj].spike_threshold) &&
	    (!(sh_mem->signal_params[jj].spike_polarity))) ) ) {
      (sh_mem->signal_params[jj].num_over_threshold)++;
      if ((sh_mem->signal_params[jj].num_over_threshold) >= NumOverThreshold) {
	sh_mem->signal_params[jj].spike_index++;
	/* spike found at this scan_index ... store scan_index for RR calculate */
	sh_mem->signal_params[jj].found_spike=sh_mem->scan_index;
	(sh_mem->signal_params[jj].since_prev_spike)=0;/* just found new spike */
	(sh_mem->signal_params[jj].num_over_threshold)=0;/* for next spike */
      }
    }	

  }
  return 0;
}


void PrintkTimeCheckStats (void) { /* func for printk jitter & task rate stats */

  int jjj; /* loop index */
  long time_usec_check[2][NUM_TIME_CHECK]; /* check time first 10 times through */

  for (jjj=1; jjj<NUM_TIME_CHECK; jjj++) {
    time_usec_check[0][jjj]=
      (long)((1e6*time_tick_check[0][jjj])/NSECS_PER_SEC);
    time_usec_check[1][jjj]=
      (long)((1e6*time_tick_check[1][jjj])/NSECS_PER_SEC);

    rtl_printf("rt_process loop %d:  time_in = %ld ticks (%ld us)
                    us since last time_in (the rt_process period) = %ld
                    time_out= %ld ticks (%ld us)
                    us since time_in (how long task itself takes) = %ld\n",
	   jjj,
	   time_tick_check[0][jjj],
	   time_usec_check[0][jjj],
	   time_usec_check[0][jjj]-time_usec_check[0][jjj-1],
	   time_tick_check[1][jjj],
	   time_usec_check[1][jjj],
	   time_usec_check[1][jjj]-time_usec_check[0][jjj]
	   );
  }

}
