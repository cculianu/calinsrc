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
#include <linux/types.h>
#include <linux/errno.h>
#include <asm/semaphore.h> /* for synchronizing the exported functions */
#include <linux/malloc.h>
#include <linux/string.h> /* some memory copyage */

#include <rtl.h>
#include <rtl_fifo.h>
#include <rtl_sched.h>
#include <rtl_sync.h>

#ifdef TIME_RT_LOOP
#  include <rtl_time.h>
#  define I_SHOULD_PRINT_TIME \
         (!(sh_mem->scan_index % (sh_mem->sampling_rate_hz * 10)))
#endif

#include <linux/comedilib.h>

#include "shared_stuff.h"     /* header file that is shared between user/kernel
				 in this system */
#include "rt_process.h"       /* header file specific to this program */

#define __I_AM_BUSY (__this_module.flags & (MOD_DELETED | MOD_INITIALIZING | MOD_JUST_FREED | MOD_UNINITIALIZED))

#define MODULE_NAME RT_PROCESS_MODULE_NAME

MODULE_AUTHOR("David J. Christini, PhD and Calin A. Culianu [not PhD :(]");
MODULE_DESCRIPTION(MODULE_NAME ": A Real-Time Sampling Task for use with kcomedilib and the daq_system user program");

MODULE_PARM(ai_fifo, "i");
MODULE_PARM_DESC(ai_fifo, "The /dev/rtfXX device number to use for echoing incoming analog input. Defaults to 0.");
MODULE_PARM(ao_fifo, "i");
MODULE_PARM_DESC(ao_fifo, "The /dev/rtfXX device numer to use for echoing outgoing analog output. Defaults to 1.");
MODULE_PARM(ai_comedi_device, "i");
MODULE_PARM_DESC(ai_comedi_device, "The comedi device minor number to use for analog input. Defaults to 0.");
MODULE_PARM(ao_comedi_device, "i");
MODULE_PARM_DESC(ao_comedi_device, "The comedi device minor number to use for analog output. Defaults to 0.");

EXPORT_SYMBOL_NOVERS(rtp_register_function);
EXPORT_SYMBOL_NOVERS(rtp_unregister_function);
EXPORT_SYMBOL_NOVERS(rtp_deactivate_function);
EXPORT_SYMBOL_NOVERS(rtp_activate_function);

int init_module(void);        /* set up RT stuff */
void cleanup_module(void);    /* clean up RT stuff */

/* sets up RT shared memory */
static int init_shared_mem(int ai_minor, int ai_subdev, int ai_fifo,
			   int ao_minor, int ao_subdev, int ao_fifo);   
/* sets up the krange cache */
static int build_krange_cache(void);

static void cleanup_krange_cache(void);
static void cleanup_fifos(void);
static void cleanup_comedi_stuff(void);
static void grabScanOffBoard (SharedMemStruct *s,
			      MultiSampleStruct *m);  /* internal */
static void putFullScanIntoAIFifo (SharedMemStruct *s, 
				   MultiSampleStruct *m);
static int __rtp_set_f_active(rtfunction_t f, char v); /* internal */
static int __rtp_register_function(rtfunction_t function); /* internal */
static int __rtp_unregister_function(rtfunction_t function); /* internal */

typedef enum SubDevT {
  AI = 0,
  AO
} SubDevT;

/* operates on krange_cache below to determine the voltage for a sample */
inline double sampl_to_volts(SubDevT t, uint chan, uint range, lsampl_t data);

static pthread_t daq_task = 0;          /* main RT task */
static SharedMemStruct *sh_mem = 0;     /* define mbuff shared memory */

struct krange_cache {
  comedi_krange *ai_ranges;
  comedi_krange *ao_ranges;
  int            ai_max_n_ranges; /* --|__ these two members are column size */
  int            ao_max_n_ranges; /* --|   for each row in above  2d arrays */
  lsampl_t      *ai_maxdatas;
  lsampl_t      *ao_maxdatas;
};

static struct krange_cache krange_cache = {0, 0, 0, 0, 0, 0};

static struct rt_function_list 
             *rt_functions, /* circularly linked-list of functions to run
			       from main rt loop */
             __end_of_func_list = {0}; /* circular linked list terminator */

DECLARE_MUTEX(rt_functions_sem);

/* module parameters */
int ai_fifo = DEFAULT_AI_FIFO, 
    ao_fifo = DEFAULT_AO_FIFO, 
    ai_comedi_device = DEFAULT_COMEDI_MINOR,
    ao_comedi_device = DEFAULT_COMEDI_MINOR;


/* this task reads data from the DAQ board, then calls 
   all of the functions registered in rt_functions linked list */
static void *daq_rt_task (void *arg) 
{
  static MultiSampleStruct one_full_scan; /* Used to pass off samples to other
					     modules */
  register hrtime_t loopstart = 0;    /* used to calibrate timing on
					 sampling rate changes            */
  register long nanos_to_sleep;

  /* below is used to put upper limit on the array in one_full_scan */
  struct rt_function_list *curr;
#ifdef TIME_RT_LOOP
  register hrtime_t looptime = 0, max = 0, min = HRTIME_INFINITY;
  uint avg_total_time = 0;
#endif

  do {
    loopstart = gethrtime();

#ifdef TIME_RT_LOOP
    if ( I_SHOULD_PRINT_TIME )
      rtl_printf("Just scanning the channels took %ld nanoseconds\n",(long int)(one_full_scan.acq_end - one_full_scan.acq_start));
#endif
      
    /* Is the rt_functions list safe to read (semaphore not busy)? 
       If not we won't run our functions this time through.. which I hope is OK
       for most people... but kind of breaks the whole rt-nature of things
       As long as you don't register and unregister functions during a critical
       time, then we are ok here */
    if (atomic_read(&rt_functions_sem.count) > 0) {
      /* iterate through all custom functions and run them now... 
         two functions are always present as they are 'hard coded':
           1) grabScanOffBoard()
	   2) putFullScanIntoAIFifo()
      */
      for (curr = rt_functions; curr != &__end_of_func_list; curr=curr->next) {
	if (curr->active_flag)
	  curr->function(sh_mem, &one_full_scan);
      }
    }

#ifdef TIME_RT_LOOP
    /*---- time code */
    looptime = gethrtime() - loopstart;
    if ( min > looptime )
      min = looptime;
    if ( max < looptime )
      max = looptime;

    avg_total_time -= 
      ((uint)(avg_total_time - looptime)) 
      / (sh_mem->scan_index + 1);


    if ( I_SHOULD_PRINT_TIME ) {
      rtl_printf("Scan %ld: "
		 "Main rt thread takes %ld nanoseconds to iterate once\n"
		 "( %ld avg / %ld min / %ld max ).\n", 
		 (long int)sh_mem->scan_index,
		 (long int)looptime, 
		 (long int)avg_total_time,
		 (long int)min,
		 (long int)max);
    }
    /*---- end time code */
#endif

    /* increment scan_index counter */
    ++sh_mem->scan_index; 

    nanos_to_sleep = 
      (BILLION/sh_mem->sampling_rate_hz) - 
      (gethrtime() - loopstart);   
  } while ( ! nanosleep(hrt2ts(nanos_to_sleep), NULL) );  /* periodic loop */
  return (void *)0;
}

int init_module(void) 
{
  pthread_attr_t attr;
  int error = -EBUSY, ai_subdev = -1, ao_subdev = -1;

  struct sched_param sched_param;
  const char *errorMessage = "Success";


  /* initialize the function linked list */
  rt_functions = __end_of_func_list.next = &__end_of_func_list;

  /* note that order of registrations is important.. the function list is 
     FIFO ordered */
  if ( (error = __rtp_register_function(grabScanOffBoard)) ||
       (error = __rtp_register_function(putFullScanIntoAIFifo)) ) {
    errorMessage = "Internal error: Cannot register a required function onto the dynamic function list! Out of memory?";
    goto init_error;
  }
    
  __rtp_set_f_active(grabScanOffBoard, 1);
  __rtp_set_f_active(putFullScanIntoAIFifo, 1);

  /* find a subdevice of type COMEDI_SUBD_AI */
  if ( (
	ai_subdev = 
	comedi_find_subdevice_by_type(ai_comedi_device, COMEDI_SUBD_AI, 0)
       ) < 0 ) {
    errorMessage = "Cannot find an analog input subdevice for the specified "
                   "minor number";
    error = ai_subdev;
    goto init_error;
  }
#ifndef NO_AO
  /* find a subdevice of type COMEDI_SUBD_AO */
  if ( (
	ao_subdev = 
	comedi_find_subdevice_by_type(ao_comedi_device, COMEDI_SUBD_AO, 0)
       ) < 0 ) {
    errorMessage = "Cannot find an analog output subdevice for the specified "
                   "minor number";
    error = ao_subdev;
    goto init_error;
  }
#else
  ao_subdev = -1;
#endif
  /* lock the subdevice  */
  if ( (error =	comedi_lock(ai_comedi_device, ai_subdev)) < 0 ) {  
    errorMessage = "Cannot lock analog input comedi subdevice";
    goto init_error;
  }

#ifndef NO_AO
  /* lock the subdevice  */
  if ( (error =	comedi_lock(ao_comedi_device, ao_subdev)) < 0 ) {  
    errorMessage = "Cannot lock analog output comedi subdevice";
    goto init_error;
  }
#endif

  if ( (error = comedi_open(ai_comedi_device)) < 0 ) {
    errorMessage = "Error opening comedi device for analog input";
    goto init_error;
  }   

#ifndef NO_AO
  if ( ai_comedi_device != ao_comedi_device &&
       (error = comedi_open(ao_comedi_device)) < 0 ) {
    errorMessage = "Error opening comedi device for analog output";
    goto init_error;
  }   
#endif 
  if (!init_shared_mem(ai_comedi_device, ai_subdev, ai_fifo,
		       ao_comedi_device, ao_subdev, ao_fifo))  {  
    /* error initializing shared memory */
    errorMessage = "Cannot allocate mbuff shared memory.  Did you create the "
                   "device node? Are you low on memory?";
    error = -ENOMEM;
    goto init_error;
  }

  /* cache the krange list... */
  if (!build_krange_cache()) {
    /* cannot allocate memory for krange cache */
    errorMessage = "Cannot allocate memory for some internal data structures. "
                   "Are we low on memory?";
    error = -ENOMEM;
  }
  
  if ( (error = rtf_create(sh_mem->ai_fifo_minor, RT_QUEUE_SZ_BYTES)) ) {
    errorMessage = "Cannot create fifo for communicating analog input to userland!";     
    goto init_error;
  }

  if ( (error = rtf_create(sh_mem->ao_fifo_minor, RT_QUEUE_SZ_BYTES)) ) {
    errorMessage = "Cannot create a fifo for communicating analog output to userland!";     
    goto init_error;
  }
  

  /* create the realtime task */
  pthread_attr_init(&attr);
  pthread_attr_setfp_np(&attr, 1);
  sched_param.sched_priority = 1;
  pthread_attr_setschedparam(&attr, &sched_param);
  if ( (error = pthread_create(&daq_task, &attr, 
			       daq_rt_task, (void *)0) ) ) {
    errorMessage = "Cannot create the daq pthread";
    goto init_error;
  }

  printk(RT_PROCESS_MODULE_NAME ": acquisition started at %d Hz (%s)\n", 
         sh_mem->sampling_rate_hz, 
         errorMessage);
  return 0;

 init_error:
  cleanup_module(); 
  printk (RT_PROCESS_MODULE_NAME ": Failure -- can't initalize RT task (%s)\n", 
	  errorMessage);
  return error;
}

/*  Initializes the rtlinux shared memory 
    returns 1 on success, 0 on failure */
static int init_shared_mem(int im, int is, int iF, int om, int os, int oF) 
{ 

  uint i;

  /* Open mbuff Shared Memory structure */
  sh_mem = 
    (SharedMemStruct *) mbuff_alloc (SHARED_MEM_NAME,
				     sizeof(SharedMemStruct));
  
  if (! sh_mem) { /* means there was some sort of error allocating mbuff */
    return 0;
  }

  /* initialize shared memory variables ... see rt_process.h for definitions */
  sh_mem->struct_version = SHD_SHM_STRUCT_VERSION;
  sh_mem->ai_minor = im;
  sh_mem->ao_minor = om;
  sh_mem->ai_subdev = is;
  sh_mem->ao_subdev = os;
  sh_mem->ai_fifo_minor = iF;
  sh_mem->ao_fifo_minor = oF;
  /* num AI channels in use */
  sh_mem->n_ai_chans = comedi_get_n_channels(sh_mem->ai_minor, 0); 
#ifndef NO_AO
  sh_mem->n_ao_chans = comedi_get_n_channels(sh_mem->ao_minor, 0);
#else
  sh_mem->n_ao_chans = 0;
#endif
  sh_mem->sampling_rate_hz = INITIAL_SAMPLING_RATE_HZ;
  sh_mem->scan_index = 0;

  for (i = 0; i < sh_mem->n_ai_chans || i < sh_mem->n_ao_chans; i++) { 
    /* set channel parameters */   
    if (i < sh_mem->n_ai_chans) {
      sh_mem->ai_chan[i] = CR_PACK(i,INITIAL_CHANNEL_GAIN,AREF_GROUND);
      set_chan(i, sh_mem->ai_chans_in_use, 0);
    }
    if (i < sh_mem->n_ao_chans) {
      set_chan(i, sh_mem->ao_chans_in_use, 0);
    }
  }

  return 1;
}

static
int build_krange_cache(void)
{
  int i, j, *ai_n_ranges = 0, *ao_n_ranges = 0, ret = 0;

  if (!(ai_n_ranges = kmalloc(sizeof(int) * sh_mem->n_ai_chans, GFP_KERNEL)) ||
      !(ao_n_ranges = kmalloc(sizeof(int) * sh_mem->n_ao_chans, GFP_KERNEL)) ||
      !(krange_cache.ai_maxdatas = kmalloc(sizeof(lsampl_t) * sh_mem->n_ai_chans, GFP_KERNEL)) ||
      !(krange_cache.ao_maxdatas = kmalloc(sizeof(lsampl_t) * sh_mem->n_ao_chans, GFP_KERNEL)) )
    goto end;
  
  /* record the number of ranges for each channel */
  for (i = 0; i < sh_mem->n_ai_chans; i++) {
    if ( (ai_n_ranges[i] = 
	  comedi_get_n_ranges(sh_mem->ai_minor, sh_mem->ai_subdev, i)) 
	 > krange_cache.ai_max_n_ranges )
      krange_cache.ai_max_n_ranges = ai_n_ranges[i];
    krange_cache.ai_maxdatas[i] = 
      comedi_get_maxdata(sh_mem->ai_minor, sh_mem->ai_subdev, i);
  }
  for (i = 0; i < sh_mem->n_ao_chans; i++) {
      if ( (ao_n_ranges[i] =
	    comedi_get_n_ranges(sh_mem->ao_minor, sh_mem->ao_subdev, i))
	   > krange_cache.ao_max_n_ranges )
	krange_cache.ao_max_n_ranges = ao_n_ranges[i];
      krange_cache.ao_maxdatas[i] = 
	comedi_get_maxdata(sh_mem->ao_minor, sh_mem->ao_subdev, i);
  }

  if (!(krange_cache.ai_ranges =
	kmalloc(sizeof(comedi_krange) * sh_mem->n_ai_chans 
		* krange_cache.ai_max_n_ranges, GFP_KERNEL)) ||
      !(krange_cache.ao_ranges =
	kmalloc	(sizeof(comedi_krange) * sh_mem->n_ao_chans 
		 * krange_cache.ao_max_n_ranges, GFP_KERNEL)))
    goto end;
 
  for (i = 0; i < sh_mem->n_ai_chans; i++) {
    for (j = 0; j < ai_n_ranges[i]; j++)
      comedi_get_krange(sh_mem->ai_minor, sh_mem->ai_subdev, i, j, 
			krange_cache.ai_ranges + i 
			* krange_cache.ai_max_n_ranges + j);
  }
  for (i = 0; i < sh_mem->n_ao_chans; i++) {
    for (j = 0; j < ao_n_ranges[i]; j++)
      comedi_get_krange(sh_mem->ao_minor, sh_mem->ao_subdev, i, j, 
			krange_cache.ao_ranges + i 
			* krange_cache.ao_max_n_ranges + j);
  }
  ret = 1;
  end:
  if (ai_n_ranges) { kfree(ai_n_ranges); ai_n_ranges = 0; }
  if (ao_n_ranges) { kfree(ao_n_ranges); ao_n_ranges = 0; }  
  return ret;
}

void cleanup_module(void) 
{ 
  /* delete daq_task */
  if (daq_task) {
    if (!pthread_delete_np(daq_task)) 
      printk (RT_PROCESS_MODULE_NAME": deleted RT task after %lu cycles.\n",
	      (unsigned long int)(sh_mem ? sh_mem->scan_index + 1 : 0));
    else 
      printk (RT_PROCESS_MODULE_NAME": cannot find RT task (it died?).\n");
  }

  __rtp_unregister_function(putFullScanIntoAIFifo);

  /* NB: clearing out of all of the other functions from the function linked 
     list is not required since if we were called here the function linked list
     must be empty since our module use count is now 0 */

  /* close all successfully opened fifos */
  cleanup_fifos();

  /* release comedi-related resources */
  cleanup_comedi_stuff();

  /* release memory for the krange */
  cleanup_krange_cache();

  if (sh_mem) {
    /* free up shared memory */
    mbuff_free(SHARED_MEM_NAME,(void *)sh_mem);
  }

}
static void cleanup_fifos (void) 
{
  if (! sh_mem) return;

  do {
    int i, fifos[2] = {sh_mem->ai_fifo_minor, sh_mem->ao_fifo_minor};
    
    for (i = 0; i < 2; i++)
      if (fifos[i] >= 0)
	rtf_destroy(fifos[i]);
  
  } while (0);
}

static void cleanup_comedi_stuff (void) 
{
  if (! sh_mem) return;

  if (sh_mem->ai_subdev >= 0) {
    /*  cancel any pending ai operation */
    comedi_cancel(sh_mem->ai_minor, sh_mem->ai_subdev);

    /* unlock subdevice so other comedi programs can use */
    comedi_unlock(sh_mem->ai_minor, sh_mem->ai_subdev);
  }

  if (sh_mem->ao_subdev >= 0) {
    /*  cancel any pending ai operation */
    comedi_cancel(sh_mem->ao_minor, sh_mem->ao_subdev);

    /* unlock subdevice so other comedi programs can use */
    comedi_unlock(sh_mem->ao_minor, sh_mem->ao_subdev);
  }

    /* close the analog input minor device */
  if (sh_mem->ai_subdev >= 0)
    comedi_close(sh_mem->ai_minor);
  
    /* close the analog output minor device (if not already closed) */
  if (sh_mem->ao_subdev >= 0 && sh_mem->ao_subdev != sh_mem->ai_subdev)
    comedi_close(sh_mem->ao_minor);

}

static 
void cleanup_krange_cache (void) 
{
  /* release the cache of kranges */
  if (krange_cache.ai_ranges) {
    kfree(krange_cache.ai_ranges);
    krange_cache.ai_ranges = 0;
  }
  if (krange_cache.ao_ranges) {
    kfree(krange_cache.ao_ranges);
    krange_cache.ao_ranges = 0;
  }
  if (krange_cache.ai_maxdatas) {
    kfree(krange_cache.ai_maxdatas);
    krange_cache.ai_maxdatas = 0;
  }
  if (krange_cache.ao_maxdatas) {
    kfree(krange_cache.ao_maxdatas);
    krange_cache.ao_maxdatas = 0;
  }

}

static void grabScanOffBoard (SharedMemStruct *s,
			      MultiSampleStruct *m)
{
  register uint i, packed_param;
  lsampl_t samp;

/*---------------------------------------------------------------------------
  Data Acquisition below...
  -------------------------
  Cheap hack to get acquisition working with new comedi.. 
  This loop is SLOW and may break timing! It takes over .5 ms to run just
  this loop! 
  (the rest of this takes circa 20-40 microseconds, so
  this by far is the bottleneck of our rt-task)
  TODO: NEED TO REWRITE THIS!! Possibly to use comedi commands, with a 
  realtime callback or somesuch.  Problem is that it's tough to get commands
  working in a really board-independent fashion. 
  (Note to self: Ok, it's not tough, but requires some probing in 
  init_module to determine some minimum values for the comedi_cmd struct)
---------------------------------------------------------------------------*/
  m->acq_start = gethrtime();
  memcpy(m->channel_mask, s->ai_chans_in_use, CHAN_MASK_SIZE);
  for (i = 0, m->n_samples = 0; i < s->n_ai_chans; i++) {
    if (is_chan_on(i, m->channel_mask)) {
      packed_param = s->ai_chan[i]; /* for shorter line length  below... */
      comedi_data_read(s->ai_minor, s->ai_subdev, 
		       CR_CHAN(packed_param),CR_RANGE(packed_param), 
		       CR_AREF(packed_param),&samp);      
      m->samples[m->n_samples].data = 
	sampl_to_volts(AI,CR_CHAN(packed_param),CR_RANGE(packed_param), samp);
      m->samples[m->n_samples].scan_index = s->scan_index;
      m->samples[m->n_samples].channel_id = i;
      m->n_samples++;
    }
  }  
  m->acq_end = gethrtime();
/*---------------------------------------------------------------------------
  /End data acquisition/
---------------------------------------------------------------------------*/
}

static void putFullScanIntoAIFifo (SharedMemStruct *s, 
				   MultiSampleStruct *m) 
{
  //  static DECLARE_FIFO_DELIMITER(delim);
  register int 
    i, 
    fifo_minor = s->ai_fifo_minor, 
    n_samples_to_write = m->n_samples;
#ifdef TIME_RT_LOOP
  static hrtime_t put_start;

  put_start = gethrtime();
#endif

  for (i = 0; i < n_samples_to_write; i++) {
  /* Note: error checking is not done here.  The fifo must meet
     the following three conditions:
     1) It must have been created with a call to rtf_create
     2) It must be a fifo that is less than RTF_NO
     3) It must have sufficient space for what we put in
     Otherwise, this put will fail! */
    if (//rtf_put(fifo_minor, &delim, sizeof(FIFO_DELIMITER_T)) < 0 || 
	rtf_put(fifo_minor, &m->samples[i], sizeof(SampleStruct)) < 0) 
      /* abort loop if we ran out of fifo to put into */
      break; 
  }
#ifdef RTP_DEBUG_FIFO_WRITES
  if (i == n_samples_to_write)
    rtl_printf("%s: rtf_length(%d)=%d, rtf_free(%d)=%d, rtf_bufsize(%d)=%d\n",
              MODULE_NAME, fifo_minor, rtf_length(fifo_minor),
  	      fifo_minor, rtf_free(fifo_minor), 
	      fifo_minor, rtf_bufsize(fifo_minor));
#endif		 
#ifdef TIME_RT_LOOP
  if ( I_SHOULD_PRINT_TIME ) {
    rtl_printf("Putting to the fifos alone took %d nanoseconds\n", 
	       gethrtime() - put_start);
  }
#endif

}

/* registers a function to be run within the rtf loop
   returns 0 on succes, ENOMEM or EBUSY on error.  
   Specified function entry won't be run (activated)
   until activate_function() is called 
   Note: Usage counter for this module is set
*/
int rtp_register_function(rtfunction_t function)
{
  int retval;

  /* if the module isn't ready yet, abort */
  if (__I_AM_BUSY) {
    return EBUSY;
  }
  retval = __rtp_register_function(function);
  if (!retval)  MOD_INC_USE_COUNT;
  return retval;

}

/* no EBUSY here, no usage increment (meant for internal registrations */
static int __rtp_register_function(rtfunction_t function)
{
  struct rt_function_list *new, *tmp, *last; 

  new = kmalloc(sizeof(struct rt_function_list), GFP_KERNEL);

  if (!new) {
    return ENOMEM;
  }
  new->active_flag = 0;
  new->function = function;
  new->next = &__end_of_func_list;
  down(&rt_functions_sem);
  /* now push this function to the end of the list; we use the semaphore
     because he don't know what other kernel threads may be doing */
  if (rt_functions == &__end_of_func_list) { 
    /* if the list was empty, just prepend */
    __end_of_func_list.next = rt_functions = new;
  } else { 
    /* otherwise find the last element and append */
    last = rt_functions;
    while ((tmp = last->next) != &__end_of_func_list) 
      last = tmp;
    last->next = new;
  }
  up(&rt_functions_sem);

  return 0;
}


/* de-registers a function that was previously registered,
   returns 0 on success, EINVAL or EBUSY on error 
   bumps down usage counter for this module
*/
int rtp_unregister_function(rtfunction_t function)
{
  int retval;

  /* if the module isn't ready yet, abort */
  if (__I_AM_BUSY) {
    return EBUSY;
  }
  retval =  __rtp_unregister_function(function);
  if (!retval) MOD_DEC_USE_COUNT;
  return retval;

}

/* no EBUSY here, no usage decrement */
static int __rtp_unregister_function(rtfunction_t function)
{
  struct rt_function_list *curr, *prev;
  char found_flg = 0;

  down(&rt_functions_sem);

  prev = &__end_of_func_list;
  while ((curr = prev->next) != &__end_of_func_list) {
    if (curr->function == function) {
      found_flg = 1;
      prev->next = curr->next;
      kfree(curr);
      continue;
    }
    prev = curr;
  }
  rt_functions = __end_of_func_list.next; /* re-establish head of list */
  up(&rt_functions_sem);

  return (found_flg ? 0 : EINVAL);
}


/* activates a function that was previously registered,
   returns 0 on success, EBUSY or EINVAL on error */
int rtp_activate_function(rtfunction_t function)
{
  /* if the module isn't ready yet, abort */
  if (__I_AM_BUSY) {
    return EBUSY;
  }

  return __rtp_set_f_active(function, 1);
}

/* de-activates a function that was previously registered,
   returns 0 on success, EBUSY or EINVAL on error */
int rtp_deactivate_function(rtfunction_t function)
{
  /* if the module isn't ready yet, abort */
  if (__I_AM_BUSY) {
    return EBUSY;
  }

  return __rtp_set_f_active(function, 0);
}

static inline struct rt_function_list *
__find_func(rtfunction_t func, struct rt_function_list *start) 
{
  struct rt_function_list *curr;

  for (curr = start; curr != &__end_of_func_list; curr = curr->next)
    if (curr->function == func) return curr;
  return 0;
}

/* sets active flag on an entry in rt_functions 
   returns 0 on success, EBUSY or EINVAL on error */
static int __rtp_set_f_active(rtfunction_t f, char v) 
{
  register struct rt_function_list *r = rt_functions;
  register int retval = EINVAL;

  down(&rt_functions_sem);
  while ((r = __find_func(f, r))) {    
    /* keep scanning for this function (as it may appear multiple times!)
       and set all instances of it to active_flag = v */
      r->active_flag = v;
      r = r->next;
      retval = 0;
  }
  up(&rt_functions_sem);
  
  return retval;
}

inline double sampl_to_volts(SubDevT t, uint chan, uint range, lsampl_t sampl)
{
  comedi_krange *krange;
  lsampl_t maxdata;
  double ret;

  if (t == AI) {
    krange = 
      krange_cache.ai_ranges + chan * krange_cache.ai_max_n_ranges + range;
    maxdata = krange_cache.ai_maxdatas[chan];
  } else {
    krange = 
      krange_cache.ao_ranges + chan * krange_cache.ao_max_n_ranges + range;
    maxdata = krange_cache.ao_maxdatas[chan];    
  }
  
  ret = ((krange->max - krange->min) * (sampl/(double)maxdata) + krange->min) 
        * 1e-6;

  if (RF_UNIT(krange->flags) == UNIT_mA) 
    ret *= 0.001;
  
  return ret;
}

#undef __I_AM_BUSY

#ifdef TIME_RT_LOOP
#  undef I_SHOULD_PRINT_TIME
#endif
