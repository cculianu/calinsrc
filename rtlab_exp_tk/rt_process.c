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

#define EXPORT_SYMTAB 1 /* <--- for annoying kernels that need this */
#include <linux/module.h> 
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/bitops.h>
#include <asm/semaphore.h> /* for synchronizing the exported functions */
#include <asm/bitops.h>    /* for set/clear bit                        */
#include <linux/malloc.h>
#include <linux/string.h> /* some memory copyage */
#include <linux/fs.h> /* for the determine_minor() functionality */

#include <rtl.h>
#include <rtl_fifo.h>
#include <rtl_sched.h>
#include <mbuff.h>

#ifdef TIME_RT_LOOP
#  include <rtl_time.h>
#  define I_SHOULD_PRINT_TIME \
         (!( ((uint)rtp_shm->scan_index) % 10000))
#endif

#include <linux/comedilib.h>
#include "shared_stuff.h"     /* header file that is shared between user/kernel
				 in this system */
#include "rt_process.h"       /* header file specific to this program */

#define __I_AM_BUSY (__this_module.flags & (MOD_DELETED | MOD_INITIALIZING | MOD_JUST_FREED | MOD_UNINITIALIZED))

#define MODULE_NAME RT_PROCESS_MODULE_NAME

#if LINUX_VERSION_CODE >= 132114
MODULE_LICENSE("GPL"); 
#endif

MODULE_AUTHOR("David J. Christini, PhD and Calin A. Culianu [not PhD :(]");
MODULE_DESCRIPTION(MODULE_NAME ": A Real-Time Sampling Task for use with kcomedilib and the daq_system user program");

MODULE_PARM(ai_device,"s");
MODULE_PARM_DESC(ai_device, "The comedi device file to use for analog input. Defaults to device file " DEFAULT_COMEDI_DEVICE ".");
MODULE_PARM(ao_device, "s");
MODULE_PARM_DESC(ao_device, "The comedi device file to use for analog output. Defaults to device file " DEFAULT_COMEDI_DEVICE ".");

EXPORT_SYMBOL_NOVERS(rtp_register_function);
EXPORT_SYMBOL_NOVERS(rtp_unregister_function);
EXPORT_SYMBOL_NOVERS(rtp_deactivate_function);
EXPORT_SYMBOL_NOVERS(rtp_activate_function);
EXPORT_SYMBOL_NOVERS(rtp_find_free_rtf);
EXPORT_SYMBOL_NOVERS(rtp_shm);
EXPORT_SYMBOL_NOVERS(spike_info);
EXPORT_SYMBOL_NOVERS(rtp_comedi_ai_dev_handle);
EXPORT_SYMBOL_NOVERS(rtp_comedi_ao_dev_handle);

int init_module(void);        /* set up RT stuff */
void cleanup_module(void) ;    /* clean up RT stuff */

/* initializes some comedi stuff. Returns 0 and sets global 'error' on error */
static int init_comedi(void);
/* sets up RT shared memory */
static int init_shared_mem(void);   
/* sets up the krange cache */
static int build_krange_cache(void) ;

/* thread to calibrate RT jitter... */
struct calib_parms { int iterations; hrtime_t period; };
static void *calibrate_jitter(void *arg);

static void cleanup_krange_cache(void);
static void cleanup_fifos(void);
static void cleanup_comedi_stuff(void);
/* RTP Registered functions... */
static void grabScanOffBoard (MultiSampleStruct *m);  
static void putFullScanIntoAIFifo (MultiSampleStruct *m); 
static void detectSpikes (MultiSampleStruct *m); 
/* /RTP Registered */
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
static void     *daq_task_stack = 0;    /* main RT task stack */

struct krange_cache {
  comedi_krange *ai_ranges;
  comedi_krange *ao_ranges;
  int            ai_max_n_ranges; /* --|__ these two members are column size */
  int            ao_max_n_ranges; /* --|   for each row in above  2d arrays */
  lsampl_t      *ai_maxdatas;
  lsampl_t      *ao_maxdatas;
};

static struct krange_cache krange_cache = {0, 0, 0, 0, 0, 0};

/* functions to be run at the end of the real-time daq loop
   they are chained on, one after the other */
struct rt_function_list {
  char active_flag;
  rtfunction_t function;
  struct rt_function_list *next;
};

static struct rt_function_list 
             *rt_functions, /* circularly linked-list of functions to run
			       from main rt loop */
             __end_of_func_list = {0}; /* circular linked list terminator */

DECLARE_MUTEX(rt_functions_sem);

/* module parameters */
char *ai_device = DEFAULT_COMEDI_DEVICE,
     *ao_device = DEFAULT_COMEDI_DEVICE;

/* exported handles to be used with comedi functions.  This abstraction of
   comedi types is needed due to different treatments of the first parameter
   to many kcomedilib functions between comedi before version 0.7.65 and 
   newer versions */
COMEDI_T rtp_comedi_ai_dev_handle = (COMEDI_T)-1, 
         rtp_comedi_ao_dev_handle = (COMEDI_T)-1;


/* used to store working values of some comedi params */
static int ai_minor = -1, ao_minor = -1, ai_subdev = -1, ao_subdev = -1;

/* Exported globals below.. */
SharedMemStruct *rtp_shm = 0;            /* (exported) mbuff shared memory */
struct spike_info spike_info;            /* Exported spike information     */

/* some vars to keep track of the rt task period/rate */
static hrtime_t task_period = 0; 

static volatile sampling_rate_t last_sampling_rate = 0;
/* readjusts the period of pthread_t daq_task based on changes in 
   rtp_shm->sampling_rate_hz, changes: task_period and last_sampling_rate */
inline int readjust_rt_task_period(void);

static MultiSampleStruct one_full_scan; /* Used to pass off samples to other
                                           modules -- not exported so as
                                           to keep the interface clean...*/

/* Used in init_module so that subroutines can report errors... */
static int error;
static const char *errorMessage = "Success";

/* this task reads data from the DAQ board, then calls 
   all of the functions registered in rt_functions linked list */
static void *daq_rt_task (void *arg) 
{
  /* below is used to put upper limit on the array in one_full_scan */
  struct rt_function_list *curr;
  register hrtime_t loopstart = 0,     /* used to calibrate timing on
                                          sampling rate changes            */
                    lastloopstart = 0;

  register int  jitter_diff = 0;

#ifdef TIME_RT_LOOP
  register hrtime_t looptime = 0, max = 0, min = HRTIME_INFINITY;
  uint avg_total_time = 0;
#endif
  
  { /* initialize the one_full_scan struct */
    uint i;

    one_full_scan.n_samples = 0;
    
    for (i = 0; i < SHD_MAX_CHANNELS; i++) 
      one_full_scan.samples[i].magic_number = SAMPLE_STRUCT_MAGIC;      
    
  }


  while (1) {
    loopstart = gethrtime();

    if (lastloopstart > 0LL) {
    /* compute jitter */
      jitter_diff = (lastloopstart + task_period) - loopstart;
      if (jitter_diff < 0) jitter_diff = -jitter_diff;
      if (((uint)jitter_diff) > rtp_shm->jitter_ns) 
        rtp_shm->jitter_ns = jitter_diff;
      
    }

    /* recomputer period in case sampling_rate changed */
    readjust_rt_task_period();


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
      for (curr = rt_functions; curr != &__end_of_func_list; curr=curr->next) 
        if (curr->active_flag)
          curr->function(&one_full_scan);
      
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
      / ((uint)(rtp_shm->scan_index + 1));


    if ( I_SHOULD_PRINT_TIME ) {
      rtl_printf("Scan %ld: "
		 "Main rt thread takes %ld nanoseconds to iterate once\n"
		 "( %ld avg / %ld min / %ld max ).\n", 
		 (long int)rtp_shm->scan_index,
		 (long int)looptime, 
		 (long int)avg_total_time,
		 (long int)min,
		 (long int)max);
    }
    /*---- end time code */
#endif

    /* increment scan_index counter */
    ++rtp_shm->scan_index; 

    lastloopstart = loopstart;
    pthread_wait_np();
  }

  return (void *)0;
}

int init_module(void) 
{
  int i;

  error = -EBUSY;
  
  /* initialize the array of last spikes encountered */
  for (i = 0; i < SHD_MAX_CHANNELS; i++)  {
    spike_info.last_spike_time[i] = 0;
    spike_info.period[i] = 0xffffffff;
  }
  memset(spike_info.spikes_this_scan, 0, CHAN_MASK_SIZE);

  /* initialize the function linked list */
  rt_functions = __end_of_func_list.next = &__end_of_func_list;

  /* note that order of registrations is important.. the function list is 
     FIFO ordered */
  if ( (error = __rtp_register_function(grabScanOffBoard)) ||
       (error = __rtp_register_function(detectSpikes)) ||
       (error = __rtp_register_function(putFullScanIntoAIFifo)) ) {
    errorMessage = "Internal error: Cannot register a required function onto the dynamic function list! Out of memory?";
    goto init_error;
  }
    
  __rtp_set_f_active(grabScanOffBoard, 1);
  __rtp_set_f_active(putFullScanIntoAIFifo, 1);
  __rtp_set_f_active(detectSpikes, 1);

  if ( init_comedi() ) goto init_error;

  if ( init_shared_mem() )  {  
    /* error initializing shared memory */
    errorMessage = "Cannot allocate mbuff shared memory.  Did you create the "
                   "device node? Are you low on memory?";
    error = -ENOMEM;
    goto init_error;
  }

  /* cache the krange list... */
  if (build_krange_cache()) {
    /* cannot allocate memory for krange cache */
    errorMessage = "Cannot allocate memory for some internal data structures. "
                   "Are we low on memory?";
    error = -ENOMEM;
  }
  
  if ((error = rtp_find_free_rtf(&rtp_shm->ai_fifo_minor, SS_RT_QUEUE_SZ_BYTES))) {
    errorMessage = "Cannot create fifo for communicating analog input to userland!";     
    goto init_error;
  }

  if ((error = rtp_find_free_rtf(&rtp_shm->ao_fifo_minor, SS_RT_QUEUE_SZ_BYTES))) {
    errorMessage = "Cannot create a fifo for communicating analog output to userland!";     
    goto init_error;
  }

  if ( !(daq_task_stack = kmalloc(RTL_PTHREAD_STACK_MIN, GFP_KERNEL)) ) {
    error = -ENOMEM;
    errorMessage = "Cannot allocate memory for the daq task's stack.";
    goto init_error;
  }

  { /* calibrate jitter... */
    pthread_attr_t attr;
    pthread_t calibration_thread;
    struct sched_param sched_param;
    struct calib_parms calib_parms = { iterations:5000, period: 200000LL };

    /* create the calibration thread */
    pthread_attr_init(&attr);
    pthread_attr_setfp_np(&attr, 1);
    pthread_attr_setstackaddr(&attr, daq_task_stack);
    pthread_attr_setstacksize(&attr, RTL_PTHREAD_STACK_MIN);
    sched_param.sched_priority = SCHED_FIFO;
    pthread_attr_setschedparam(&attr, &sched_param);  
    if ( (error = pthread_create(&calibration_thread, &attr, 
                                 calibrate_jitter, &calib_parms) ) ) {
      errorMessage = "Cannot create the jitter calibration pthread";
      goto init_error;
    }
    printk(RT_PROCESS_MODULE_NAME": Calibrating realtime jitter...\n");
    pthread_join(calibration_thread, 0);
  }

  printk(RT_PROCESS_MODULE_NAME": Jitter calibrated to %u nanoseconds\n", 
         rtp_shm->jitter_ns);

  { /* create the main daq RT thread.. */
    pthread_attr_t attr;
    struct sched_param sched_param;

    /* create the realtime task */
    pthread_attr_init(&attr);
    pthread_attr_setfp_np(&attr, 1);
    pthread_attr_setstackaddr(&attr, daq_task_stack);
    pthread_attr_setstacksize(&attr, RTL_PTHREAD_STACK_MIN);
    sched_param.sched_priority = SCHED_FIFO;
    pthread_attr_setschedparam(&attr, &sched_param);  
    if ( (error = pthread_create(&daq_task, &attr, 
                                 daq_rt_task, (void *)0) ) ) {
      errorMessage = "Cannot create the daq pthread";
      goto init_error;
    }
  }

  /* assumption for readjust_rt_task_period () is rtp_shm is initialized.. */
  if ( ( error = readjust_rt_task_period() ) ) {
    errorMessage = "Cannot make DAQ RT Task periodic";
    goto init_error;
  }

  printk(RT_PROCESS_MODULE_NAME ": acquisition started at %d Hz (%s)\n", 
         rtp_shm->sampling_rate_hz, 
         errorMessage);
  return 0;

 init_error:
  cleanup_module(); 
  printk (RT_PROCESS_MODULE_NAME ": Failure -- can't initalize RT task (%s)\n", 
	  errorMessage);
  return error;
}

static void *calibrate_jitter(void *arg) 
{
  const int iterations = ((struct calib_parms *)arg)->iterations;
  const hrtime_t period = ((struct calib_parms *)arg)->period;
  hrtime_t last_iter = 0, max_jitter = 0, curr_jitter = 0;
  int i; 

  pthread_make_periodic_np(pthread_self(), gethrtime() + period, period);

  for (i = 0; i < iterations; i++, pthread_wait_np()) {
    curr_jitter = gethrtime() - (last_iter + period);
    if (last_iter > 0LL) {
      if (curr_jitter < 0) curr_jitter = -curr_jitter;
      if (curr_jitter > max_jitter) max_jitter = curr_jitter;
    }
    last_iter = gethrtime();
  }
  
  rtp_shm->jitter_ns = max_jitter;

  pthread_exit((void *)rtp_shm->jitter_ns);
  return (void *)rtp_shm->jitter_ns;
}

/* returns error if file is not a (configured) comedi device */
static int determine_comedi_minor(const char *file)
{
  struct nameidata nd;
  int ret;

  if (!file || !*file) return -EINVAL;
  if (!path_init("/", LOOKUP_FOLLOW|LOOKUP_POSITIVE, &nd)) 
    { ret = -ENOENT; goto release; }
  if ( (ret = path_walk(file, &nd)) ) return ret;
  if (!S_ISCHR(nd.dentry->d_inode->i_mode)) { ret = -ENODEV; goto release; }
  if (MAJOR(nd.dentry->d_inode->i_rdev) != COMEDI_MAJOR) { 
    ret = -ENODEV; 
    goto release;
  }
  
  ret = MINOR(nd.dentry->d_inode->i_rdev);
 release:
  path_release(&nd);
  return ret;
}

static int
init_comedi(void)
{
  ai_minor = determine_comedi_minor(ai_device);
  if (ai_minor < 0) {
    error = ai_minor;
    errorMessage = "Error opening the ANALOG INPUT device.  Please verify "
      "that you passed the right ai_device module parameter to rt_process.o!";
    return error;
  }

  ao_minor = determine_comedi_minor(ao_device); 
  if (ao_minor < 0) {
    error = ai_minor;
    errorMessage = "Error opening the ANALOG OUTPUT device.  Please verify "
      "that you passed the right ao_device module parameter to rt_process.o!";
    return error;
  }

#ifdef NEW_STYLE_KCOMEDILIB
  {
    char tempbuf[32];

    sprintf(tempbuf, "/dev/comedi%d", ai_minor);

    rtp_comedi_ai_dev_handle = comedi_open(tempbuf);

    if (!rtp_comedi_ai_dev_handle) {
      rtp_comedi_ai_dev_handle = (COMEDI_T)-1;
      errorMessage = "Error opening comedi device for analog input";
      error = -ENOMSG;
      return error;
    }  

    if (ai_minor != ao_minor) {
      sprintf(tempbuf, "/dev/comedi%d", ao_minor);    
      rtp_comedi_ao_dev_handle = comedi_open(tempbuf);
      if ( !rtp_comedi_ao_dev_handle ) {
        rtp_comedi_ao_dev_handle = (COMEDI_T)-1;
        errorMessage = "Error opening comedi device for analog output";
        error = -ENOMSG;
        return error;
      } 
    }
    else
      rtp_comedi_ao_dev_handle = rtp_comedi_ai_dev_handle;

  } 
#else
  rtp_comedi_ai_dev_handle = ai_minor;
  rtp_comedi_ao_dev_handle = ao_minor;
#endif

  /* find a subdevice of type COMEDI_SUBD_AI */
  if ( (
	ai_subdev = 
	comedi_find_subdevice_by_type(rtp_comedi_ai_dev_handle, COMEDI_SUBD_AI, 0)
       ) < 0 ) {
    errorMessage = "Cannot find an analog input subdevice for the specified "
                   "minor number";
#ifdef OLD_STYLE_KCOMEDILIB
    /* indicate the device isn't really open */
    rtp_comedi_ai_dev_handle = rtp_comedi_ao_dev_handle = -1;
#endif
    error = ai_subdev;
    return error;
  }

  /* find a subdevice of type COMEDI_SUBD_AO */
  if ( (
	ao_subdev = 
	comedi_find_subdevice_by_type(rtp_comedi_ao_dev_handle, COMEDI_SUBD_AO, 0)
       ) < 0 ) {
#ifdef OLD_STYLE_KCOMEDILIB
    /* indicate the device isn't really open */
    rtp_comedi_ai_dev_handle = rtp_comedi_ao_dev_handle = -1;
#endif
    errorMessage = "Cannot find an analog output subdevice for the specified "
                   "minor number";
    error = ao_subdev;
    return error;
  }

  /* lock the subdevice  */
  if ( (error =	comedi_lock(rtp_comedi_ai_dev_handle, ai_subdev)) < 0 ) {  
    errorMessage = "Cannot lock analog input comedi subdevice";
#ifdef OLD_STYLE_KCOMEDILIB
    /* indicate the device isn't really open */
    rtp_comedi_ai_dev_handle = rtp_comedi_ao_dev_handle = -1;
#endif
    return error;
  }

  /* lock the subdevice  */
  if ( (error =	comedi_lock(rtp_comedi_ao_dev_handle, ao_subdev)) < 0 ) {  
    errorMessage = "Cannot lock analog output comedi subdevice";
#ifdef OLD_STYLE_KCOMEDILIB
    /* unlock it so that cleanup_comedi_stuff() isn't confused */
    comedi_unlock(rtp_comedi_ai_dev_handle, ai_subdev);
    /* indicate the device isn't really open */
    rtp_comedi_ai_dev_handle = rtp_comedi_ao_dev_handle = -1;
#endif
    return error;
  }

#ifdef OLD_STYLE_KCOMEDILIB
  if ( (error = comedi_open(ai_minor)) < 0 ) {
    comedi_unlock(ai_minor, ai_subdev);
    comedi_unlock(ao_minor, ao_subdev);
    rtp_comedi_ai_dev_handle = rtp_comedi_ao_dev_handle = -1;
    errorMessage = "Error opening comedi device for analog input";
    return error;
  }  
  rtp_comedi_ai_dev_handle = ai_minor;
  
  if ( ai_minor != ao_minor &&
       (error = comedi_open(ao_minor)) < 0 ) {
    comedi_unlock(ai_minor, ai_subdev);
    comedi_unlock(ao_minor, ao_subdev);
    comedi_close(ai_minor);
    rtp_comedi_ai_dev_handle = rtp_comedi_ao_dev_handle = -1;    
    errorMessage = "Error opening comedi device for analog output";
    return error;
  }   
  rtp_comedi_ao_dev_handle = ao_minor;  
#endif

  return 0;
}

/*  Initializes the rtlinux shared memory 
    returns 1 on success, 0 on failure */
static int 
init_shared_mem(void) 
{ 
  uint i;

  /* Open mbuff Shared Memory structure */
  rtp_shm = 
    (SharedMemStruct *) mbuff_alloc (SHARED_MEM_NAME,
                                     sizeof(SharedMemStruct));
  
  if (! rtp_shm) { /* means there was some sort of error allocating mbuff */
    return -ENOMEM;
  }

  /* initialize shared memory variables ... see rt_process.h for definitions */
  rtp_shm->struct_version = SHD_SHM_STRUCT_VERSION;
  rtp_shm->ai_minor = ai_minor;
  rtp_shm->ao_minor = ao_minor;
  rtp_shm->ai_subdev = ai_subdev;
  rtp_shm->ao_subdev = ao_subdev;
  rtp_shm->ai_fifo_minor = -1;
  rtp_shm->ao_fifo_minor = -1;
  /* num AI channels in use */
  rtp_shm->n_ai_chans = comedi_get_n_channels(rtp_comedi_ai_dev_handle, ai_subdev); 
  rtp_shm->n_ao_chans = comedi_get_n_channels(rtp_comedi_ao_dev_handle, ao_subdev);
  rtp_shm->sampling_rate_hz = INITIAL_SAMPLING_RATE_HZ;
  rtp_shm->scan_index = 0;

  /* initialize the spike_params member */
  init_spike_params(&rtp_shm->spike_params);
  
  for (i = 0; i < rtp_shm->n_ai_chans || i < rtp_shm->n_ao_chans; i++) { 
    /* set channel parameters */   
    if (i < rtp_shm->n_ai_chans) {
      rtp_shm->ai_chan[i] = CR_PACK(i,INITIAL_CHANNEL_GAIN,AREF_GROUND);
      set_chan(i, rtp_shm->ai_chans_in_use, 0);
    }
    if (i < rtp_shm->n_ao_chans) {
      set_chan(i, rtp_shm->ao_chans_in_use, 0);
    }
  }

  return 0;
}

static
int  build_krange_cache(void)
{
  int i, j, *ai_n_ranges = 0, *ao_n_ranges = 0, ret = -1;

  if (!(ai_n_ranges = kmalloc(sizeof(int) * rtp_shm->n_ai_chans, GFP_KERNEL)) ||
      !(ao_n_ranges = kmalloc(sizeof(int) * rtp_shm->n_ao_chans, GFP_KERNEL)) ||
      !(krange_cache.ai_maxdatas = kmalloc(sizeof(lsampl_t) * rtp_shm->n_ai_chans, GFP_KERNEL)) ||
      !(krange_cache.ao_maxdatas = kmalloc(sizeof(lsampl_t) * rtp_shm->n_ao_chans, GFP_KERNEL)) )
    goto end;
  
  /* record the number of ranges for each channel */
  for (i = 0; i < rtp_shm->n_ai_chans; i++) {
    if ( (ai_n_ranges[i] = 
	  comedi_get_n_ranges(rtp_comedi_ai_dev_handle, rtp_shm->ai_subdev, i)) 
	 > krange_cache.ai_max_n_ranges )
      krange_cache.ai_max_n_ranges = ai_n_ranges[i];
    krange_cache.ai_maxdatas[i] = 
      comedi_get_maxdata(rtp_comedi_ai_dev_handle, rtp_shm->ai_subdev, i);
  }
  for (i = 0; i < rtp_shm->n_ao_chans; i++) {
      if ( (ao_n_ranges[i] =
	    comedi_get_n_ranges(rtp_comedi_ao_dev_handle, rtp_shm->ao_subdev, i))
	   > krange_cache.ao_max_n_ranges )
	krange_cache.ao_max_n_ranges = ao_n_ranges[i];
      krange_cache.ao_maxdatas[i] = 
	comedi_get_maxdata(rtp_comedi_ao_dev_handle, rtp_shm->ao_subdev, i);
  }

  if (!(krange_cache.ai_ranges =
	kmalloc(sizeof(comedi_krange) * rtp_shm->n_ai_chans 
		* krange_cache.ai_max_n_ranges, GFP_KERNEL)) ||
      !(krange_cache.ao_ranges =
	kmalloc	(sizeof(comedi_krange) * rtp_shm->n_ao_chans 
		 * krange_cache.ao_max_n_ranges, GFP_KERNEL)))
    goto end;
 
  for (i = 0; i < rtp_shm->n_ai_chans; i++) {
    for (j = 0; j < ai_n_ranges[i]; j++)
      comedi_get_krange(rtp_comedi_ai_dev_handle, rtp_shm->ai_subdev, i, j, 
			krange_cache.ai_ranges + i 
			* krange_cache.ai_max_n_ranges + j);
  }
  for (i = 0; i < rtp_shm->n_ao_chans; i++) {
    for (j = 0; j < ao_n_ranges[i]; j++)
      comedi_get_krange(rtp_comedi_ao_dev_handle, rtp_shm->ao_subdev, i, j, 
			krange_cache.ao_ranges + i 
			* krange_cache.ao_max_n_ranges + j);
  }
  ret = 0;
  end:
  if (ai_n_ranges) { kfree(ai_n_ranges); ai_n_ranges = 0; }
  if (ao_n_ranges) { kfree(ao_n_ranges); ao_n_ranges = 0; }  
  return ret;
}

void cleanup_module(void) 
{ 
  /* delete daq_task */
  if (daq_task) {
    if ( pthread_cancel(daq_task) ) 
      printk (RT_PROCESS_MODULE_NAME": cannot cancel RT task (it died?).\n");
    else if ( pthread_join(daq_task, 0) )
      printk (RT_PROCESS_MODULE_NAME": cannot join to RT thread! Argh!\n");
    else 
      printk (RT_PROCESS_MODULE_NAME": deleted RT task after %lu cycles.\n",
              (unsigned long int)(rtp_shm ? rtp_shm->scan_index : 0));    
  }

  /* delete the daq_task's stack */
  if (daq_task_stack) {
    kfree(daq_task_stack);
    daq_task_stack = 0;
  }

  __rtp_unregister_function(detectSpikes);
  __rtp_unregister_function(putFullScanIntoAIFifo);
  __rtp_unregister_function(grabScanOffBoard);

  /* NB: clearing out of all of the other functions from the function linked 
     list is not required since if we were called here the function linked list
     must be empty since our module use count is now 0 */

  /* close all successfully opened fifos */
  cleanup_fifos();

  /* release comedi-related resources */
  cleanup_comedi_stuff();

  /* release memory for the krange */
  cleanup_krange_cache();

  if (rtp_shm) 
    /* free up shared memory */
    mbuff_free(SHARED_MEM_NAME,(void *)rtp_shm);
  
}

static void cleanup_fifos (void) 
{
  if (! rtp_shm) return;

  if (rtp_shm->ai_fifo_minor >= 0) rtf_destroy(rtp_shm->ai_fifo_minor);
  if (rtp_shm->ao_fifo_minor >= 0) rtf_destroy(rtp_shm->ao_fifo_minor);    

  rtp_shm->ai_fifo_minor = rtp_shm->ao_fifo_minor = -1;
}

static void cleanup_comedi_stuff (void) 
{
  if (((int)rtp_comedi_ai_dev_handle) != -1 && ai_subdev >= 0) {
    /*  cancel any pending ai operation */
    comedi_cancel(rtp_comedi_ai_dev_handle, ai_subdev);

    /* unlock subdevice so other comedi programs can use */
    comedi_unlock(rtp_comedi_ai_dev_handle, ai_subdev);
  }

  if (((int)rtp_comedi_ao_dev_handle) != -1 && ao_subdev >= 0) {
    /*  cancel any pending ai operation */
    comedi_cancel(rtp_comedi_ao_dev_handle, ao_subdev);

    /* unlock subdevice so other comedi programs can use */
    comedi_unlock(rtp_comedi_ao_dev_handle, ao_subdev);
  }

    /* close the analog input minor device */
  if (((int)rtp_comedi_ai_dev_handle) != -1) {

    comedi_close(rtp_comedi_ai_dev_handle);
  }  
    /* close the analog output minor device (if not already closed) */
  if (((int)rtp_comedi_ao_dev_handle) != ((int)rtp_comedi_ai_dev_handle) ) {
    comedi_close(rtp_comedi_ao_dev_handle);
  }
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

static void grabScanOffBoard (MultiSampleStruct *m)
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
  memcpy(m->channel_mask, (const char *)rtp_shm->ai_chans_in_use, CHAN_MASK_SIZE);
  m->acq_start = gethrtime();
  for (i = 0, m->n_samples = 0; i < rtp_shm->n_ai_chans; i++) {
    if (is_chan_on(i, m->channel_mask)) {
      packed_param = rtp_shm->ai_chan[i]; /* for shorter line length  below */
      comedi_data_read(rtp_comedi_ai_dev_handle, rtp_shm->ai_subdev, 
                       CR_CHAN(packed_param),CR_RANGE(packed_param), 
                       CR_AREF(packed_param),&samp);      
      m->samples[m->n_samples].data = 
        sampl_to_volts(AI,CR_CHAN(packed_param),CR_RANGE(packed_param), samp);
      m->samples[m->n_samples].scan_index = rtp_shm->scan_index;
      m->samples[m->n_samples].channel_id = i;
      m->samples[m->n_samples].spike = 0;
      m->n_samples++;
    }
  }  
  m->acq_end = gethrtime();
/*---------------------------------------------------------------------------
  /End data acquisition/
---------------------------------------------------------------------------*/
}

static void detectSpikes (MultiSampleStruct *m)
{
  register uint chan, i, elapsed;
  register const uint avg_chan_time =
    ( m->n_samples  ? ((uint)(m->acq_end - m->acq_start)) / m->n_samples : 1 );
  register hrtime_t this_chan_time;  
  
  for (i = 0; i < m->n_samples; i++) {
    chan = m->samples[i].channel_id;
    this_chan_time = m->acq_start + (hrtime_t)(avg_chan_time * i);
    elapsed = (uint)(this_chan_time - spike_info.last_spike_time[chan]);

    if (test_bit(chan, rtp_shm->spike_params.enabled_mask) 
        &&  elapsed  >= rtp_shm->spike_params.blanking[chan] * MILLION ) {
      switch (_test_bit(chan, rtp_shm->spike_params.polarity_mask)) {
      case Positive: /* from enum SpikePolarity */
        m->samples[i].spike = 
          rtp_shm->spike_params.threshold[chan] <= m->samples[i].data;
        break;
      case Negative: 
        m->samples[i].spike = 
          rtp_shm->spike_params.threshold[chan] >= m->samples[i].data;
        break;
      default:
        /* this case is not reached, but defensive rtl_printf follows */
        rtl_printf(MODULE_NAME ": Bug in rt_process.o, channel %u has an "
                   "illegal spike polarity!\n", chan); 
        break;
      }

      if (m->samples[i].spike) {
        m->samples[i].spike_period = spike_info.period[chan] =
          elapsed * ((double)0.000001);
        spike_info.last_spike_time[chan] = this_chan_time;
      }
      
    }
    if (m->samples[i].spike)
      set_bit(chan, spike_info.spikes_this_scan);
    else
      clear_bit(chan, spike_info.spikes_this_scan);
  }
}

static void putFullScanIntoAIFifo (MultiSampleStruct *m) 
{
  //  static DECLARE_FIFO_DELIMITER(delim);
  register int 
    i, 
    fifo_minor = rtp_shm->ai_fifo_minor, 
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
    return -ENOMEM;
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
    return -EBUSY;
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

  return (found_flg ? 0 : -EINVAL);
}


/* activates a function that was previously registered,
   returns 0 on success, EBUSY or EINVAL on error */
int rtp_activate_function(rtfunction_t function)
{
  /* if the module isn't ready yet, abort */
  if (__I_AM_BUSY) {
    return -EBUSY;
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
  register int retval = -EINVAL;

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

/* Finds a free fifo, calls rtf_create(), and puts the minor number in minor.  
   On error a negative errno is returned. 
   Use this to quickly find a free fifos.  Helpful so that don't have to loop
   to find a free. */
int rtp_find_free_rtf(int *minor, int size)
{
  unsigned int i;

  if (!minor) return -EFAULT;
     
  /* keep trying to create a fifo until we find a free one */
  for (i = 0; i < RTF_NO; i++) {
    int ret;

    ret = rtf_create(i, size);

    /* the rtf_create docs contradict the actual code for rtf_create... */
    if (ret == size || ret == 0) {
      /* got it! */      
      *minor = i;
      printk(MODULE_NAME ": rtp_find_free_rtf(): fifo minor is %d\n", *minor);
      return 0;
    } else if (ret != -EBUSY) return ret;
  }
  
  return -EBUSY;

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


inline int readjust_rt_task_period(void)
{
  static const hrtime_t undersleep = 0LL;
  if (rtp_shm->sampling_rate_hz != last_sampling_rate) {
    /* user changed sampling rate -- DANGEROUS if set too high!!! */
    last_sampling_rate = rtp_shm->sampling_rate_hz;
    task_period = ((1000.0 / (double)last_sampling_rate) * MILLION);
    /* now re-tune the period, note that this is dangerous if set too
       fast, as the user task may never get to run! */
    return pthread_make_periodic_np (daq_task, gethrtime() + task_period  - undersleep, 
                                     task_period  - undersleep);
  }
  return 0;
}
#undef __I_AM_BUSY

#ifdef TIME_RT_LOOP
#  undef I_SHOULD_PRINT_TIME
#endif
