/*
 * This file is part of the RT-Linux Multichannel Data Acquisition System
 *
 * Copyright (C) 1999,2000 David Christini
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


#include <linux/module.h> 
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/errno.h>

#include <rtl.h>
#include <rtl_fifo.h>
#include <rtl_sched.h>
#include <rtl_sync.h>

#ifdef TIME_RT_LOOP
#  include <rtl_time.h>
#endif

#include <linux/comedilib.h>

/* define the below if you are feeling lucky, punk 
   otherwise undef it for a more reliable but slower read method in source */  
//#define FANCYPANTS_READ_METHOD

#include "rt_process.h"       /* header file specific to this program */

MODULE_AUTHOR("David Christini and Calin Culianu");
MODULE_DESCRIPTION("A Real-Time Sampling Task for use with kcomedilib");


int init_module(void);        /* set up RT stuff */
void cleanup_module(void);    /* clean up RT stuff */
static int init_shared_mem(void);   /* set up RT shared memory */
static void cleanup_fifos(void);
static int SpikeDetect(void); //func for detecting spikes
static void cleanup_comedi_stuff(void);

static pthread_t daq_task = 0;        /* main RT task */
static volatile SharedMemStruct *sh_mem = 0; /* define mbuff shared memory */

#ifdef FANCYPANTS_DATA_READ_METHOD 
static comedi_trig ai_trig;          /* main comedi trigger structure */
#endif
static FIFO_struct ai_fifo_struct; /* holds data array scanned from DAQ card */
                                   /* all channels scanned at once then placed */
                                   /* onto one FIFO in one rtf_put */
static int
#ifdef FANCYPANTS_DATA_READ_METHOD 
  ioctl_check=0,         /* flag variable to check comedi_trig_ioctl success */
#endif
  ret, 
  numGetFifos,   /* Represents the number of successfully opened fifos */
  numPutFifos,
  ai_subdev;

static const unsigned int
       getFifos[NUM_GET_FIFOS] = GET_FIFOS, /* Our set of get fifos*/
       putFifos[NUM_PUT_FIFOS] = PUT_FIFOS; /* Our set of put fifos*/

static void *daq(void *arg) { /* this task reads and writes data to a DAQ board */
  int i; /*  loop index */

#ifndef FANCYPANTS_READ_METHOD
  register unsigned int packed_param; /* temp var from sh_mem->ai_chan[i] */
#endif

#ifdef TIME_RT_LOOP
  register hrtime_t loopstart = gethrtime();
  register int already_got_time = 0;
#endif

  while (1) { /* must loop, b/c it is a periodic task */

   
    sh_mem->scan_index++; /* increment scan_index counter */
    /* Analog Input ... all channels in one ioctl call */
    
    /* gain of ai_trig is changed via sh_mem->ai_chan in non-rt-process */
    /*   __comedi_trig_ioctl instead of comedi_trig_ioctl b/c  */
    /*   it doesn't have as much overhead and is therefore faster */
    /*   doesn't initialize the board every time */

    /* doesn't work in new comedi.. at least not until i mess with it :( -cc  */
#ifdef FANCYPANTS_READ_METHOD    
    ioctl_check = comedi_trigger(AI_DEV,ai_subdev,&ai_trig);
#else
    /* cheap hack to get this working with new comedi.. 
       this may be SLOW and may break timing! */
    for (i = 0; i < sh_mem->num_ai_chan_in_use; i++) {
      packed_param = sh_mem->ai_chan[i]; /* for shorter line length below... */
      comedi_data_read(AI_DEV, ai_subdev, CR_CHAN(packed_param), CR_RANGE(packed_param), CR_AREF(packed_param), ai_fifo_struct.data + i ); 
    }
#endif      
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

#ifdef TIME_RT_LOOP
    if (!already_got_time) {
      rtl_printf("Main rt thread takes %d nanoseconds to iterate once.\n",
		 gethrtime() - loopstart);
      already_got_time = 1;
    }
#endif

    pthread_wait_np();             /* wait until beginning of next period */

  }
  return (void *)0;
}

int init_module(void) {
  pthread_attr_t attr;
  int error = -EBUSY;
  struct sched_param sched_param;
  const char *errorMessage = "Success";

  if ( (ai_subdev = 
	comedi_find_subdevice_by_type(AI_DEV,COMEDI_SUBD_AI,0)) < 0 ) {
    errorMessage = "Cannot find an analog input subdevice for the specified minor number";
    error = ai_subdev;
    goto init_error;
  }

  if ( (error = comedi_lock(AI_DEV,ai_subdev)) < 0     /* lock subdevice  */
       || (error = comedi_lock(AO_DEV,AO_SUBDEV)) < 0  /* lock subdevice  */
       ) {
    errorMessage = "Cannot lock comedi subdevice";
    goto init_error;
  }

  if ( (error = comedi_open(AI_DEV)) < 0 ) {
    errorMessage = "Error opening comedi device";
    goto init_error;
  }   

 
  if (!init_shared_mem())  {  
    /* error initializing shared memory */
    errorMessage = "Cannot allocate mbuff shared memory.  Did you create the device node? Are you asking for too much memory?";
    error = -ENOMEM;
    goto init_error;
  }

  /*  creates rt FIFOs as defined in rt_process.h */
  for (numGetFifos = 0, numPutFifos = 0; 
       numGetFifos < NUM_GET_FIFOS || numPutFifos < NUM_PUT_FIFOS ; 
       numGetFifos++, numPutFifos++) {

    if ( (numGetFifos < NUM_GET_FIFOS) 
	 && (error = rtf_create(getFifos[numGetFifos], RT_QUEUE_SIZE))
       ) {
      errorMessage = "Cannot create a get fifo!";     
      goto init_error;
    }
    if ( (numPutFifos < NUM_PUT_FIFOS) 
	 && (error = rtf_create(putFifos[numPutFifos], RT_QUEUE_SIZE))
       ) { 
      errorMessage = "Cannot create a put fifo!";
      goto init_error;
    }
  }
#ifdef FANCYPANTS_READ_METHOD
  /* set up trigger structure for ai ... see comedi documentation for specifics */
  ai_trig.subdev = ai_subdev;
  ai_trig.mode = 0;
  ai_trig.flags = 0;  /* used to be TRIG_RT -cc */
  ai_trig.n_chan = sh_mem->num_ai_chan_in_use;  /* # chans to scan each ioctl() */
  ai_trig.chanlist = (int *)sh_mem->ai_chan; /* use sh_mem so non RT process can  */
                                        /* change channel gain via CR_PACK */
  ai_trig.data=ai_fifo_struct.data;     /* array scans are placed into */
  ai_trig.n=1;
  ai_trig.data_len = ai_trig.n_chan * ai_trig.n * sizeof(sampl_t);
  ai_trig.trigsrc=0; /* used to be TRIG_NONE -cc */
  ai_trig.trigvar=0;
  ai_trig.trigvar1=0;
#endif
  /* create the realtime task */
  pthread_attr_init(&attr);
  sched_param.sched_priority = SCHED_RR; // realtime scheduling

  pthread_attr_setschedparam(&attr, &sched_param);
  if ( (error = pthread_create(&daq_task, &attr, daq, (void *)0) ) ) {
    errorMessage = "Cannot create the daq pthread";
    goto init_error;
  }

  /* make task periodic: run at a rate of RT_HZ */
  if ( (error = 
	pthread_make_periodic_np(daq_task, 
				 gethrtime(), NSECS_PER_SEC/RT_HZ) ) ) {
    errorMessage = "Cannot make daq thread periodic";
    goto init_error;
  }

  rtl_printf(RT_PROCESS_SHM_NAME ": acquisition started at %d Hz (%s)\n", 
	     RT_HZ, 
	     errorMessage);
  return 0;

 init_error:
  cleanup_module(); 
  rtl_printf (RT_PROCESS_SHM_NAME ": Failure -- can't initalize RT task (%s)\n", 
	      errorMessage);
  return error;
}

/* returns 1 on success, 0 on failure */
static int init_shared_mem(void) { /*  Initialize the rtlinux shared memory */

  int i;

  /* Open mbuff Shared Memory structure */
  sh_mem = 
    (volatile SharedMemStruct*) mbuff_alloc (RT_PROCESS_SHM_NAME,
					     sizeof(SharedMemStruct));
  
  if (! sh_mem) { /* means there was some sort of error allocating mbuff */
    return 0;
  }

  /* initialize shared memory variables ... see rt_process.h for definitions */
  sh_mem->ai_subdev = ai_subdev;
  sh_mem->scan_index = 0;
  sh_mem->num_ai_chan_in_use = NUM_AD_CHANNELS_TO_USE; /* num AI channels in use */

  for (i=0; i < sh_mem->num_ai_chan_in_use ; i++) { /* set channel parameters */
    sh_mem->signal_params[i].channel_gain = INITIAL_CHANNEL_GAIN;
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
  return 1;
}

void cleanup_module(void) {

  /* close all successfully opened fifos */
  cleanup_fifos();

  /* release comedi-related resources */
  cleanup_comedi_stuff();

  /* delete daq_task */
  if (daq_task) {
    pthread_delete_np(daq_task);
    rtl_printf(RT_PROCESS_SHM_NAME ": deleted RT task after %d cycles (~%d seconds).\n",
	       (sh_mem ? sh_mem->scan_index : 0), 
	       (sh_mem ? sh_mem->scan_index/RT_HZ: 0));
  }

  if (sh_mem) {
    /* free up shared memory */
    mbuff_free(RT_PROCESS_SHM_NAME,(void *)sh_mem);
  }

}

static int SpikeDetect(void) {
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

static void cleanup_fifos (void) {

  /*  destroys rt FIFOs as defined in rt_process.h */
  while (numGetFifos > 0) {
    numGetFifos--;
    rtf_destroy(getFifos[numGetFifos]);
  }

  while (numPutFifos > 0) {
    numPutFifos--;
    rtf_destroy(putFifos[numPutFifos]);
  }
 
}

static void cleanup_comedi_stuff (void) {
  /*  cancel any pending ai operation */
  comedi_cancel(AI_DEV,ai_subdev);

  /* unlock subdevice so other comedi programs can use */
  comedi_unlock(AI_DEV,ai_subdev);

  /* unlock subdevice so other comedi programs can use */
  comedi_unlock(AO_DEV,AO_SUBDEV);

  comedi_close(AI_DEV);

}
