/*
 * This file is part of the RT-Linux Multichannel Data Acquisition System
 *
 * Copyright (C) 1999,2000 David Christini
 * Copyright (C) 2001,2002 Calin Culianu
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
#include <linux/proc_fs.h>

#include <linux/comedilib.h>

#include "rt_process.h"
#include "rtos_middleman.h"
#include "map_control.h"
#include "proc_macros.h"

#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL"); 
#endif

/*----------------------------------------------------------------------------
  Some Private Structures...
-----------------------------------------------------------------------------*/
#define MC_N_SAVED_VALS MC_NUM_RR_AVG_MAX

struct RRIntervals {
  int intervals [MC_N_SAVED_VALS]; /* in milliseconds */
  int index; /* index of last value in above array */
  int n_intervals; /* the number of intervals we have saved */
  scan_index_t last_scan_index;
};

typedef struct RRIntervals RRIntervals;

/*  Some inline 'macros' related to the above struct */
static inline int rri_next_index(void);
static inline int rri_prev_index(void);
static inline int rri_rel_index (int);
/*---------------------------------------------------------------------------*/

#define MODULE_NAME "MC Stim"

MODULE_AUTHOR("David J. Christini, PhD and Calin A. Culianu [not PhD :(]");
MODULE_DESCRIPTION(MODULE_NAME ": A Real-Time stimulation and control add-on for daq system and rtlab.o.\n$Id$");

int map_control_init(void); 
void map_control_cleanup(void);

static int init_shared_mem(void);
static int init_ao_chan(void);

static void do_map_control_stuff(MultiSampleStruct *);

/* 'inner' helper functions... */
static void do_pre_control_stuff(void);
static void calc_rr(void);  
static void calc_stim(void); 
static void stimulate(int new_stim); /* if true, a new stim is started */
static void calc_g(void);
static void out_to_fifo(void);
static int find_free_chan(volatile char *, unsigned int);
/* called whenever the /proc/rtlab/map_control proc file is read  */
static int map_control_proc_read (char *, char **, off_t, int, int *, void *data);

module_init(map_control_init);
module_exit(map_control_cleanup);

static MCShared *shm = 0;

static RRIntervals  rr_intervals;
static MCLiebnitz  working_liebnitz; /* used for inter-function comm.      */
static int          last_ao_chan_used = -1;
static unsigned int ao_range = 0;
static lsampl_t     ao_stim_level = 0; /* basically comedi_get_maxdata()- 1 */

#define NEXT_SAVED    do { rr_intervals.index = rri_next_index(); } while(0)
#define PREV_SAVED    do { rr_intervals.index = rri_prev_index(); } while(0)
#define MOV_SAVED(x)  do { rr_intervals.index = rri_rel_index(x); } while(0)
#define RRI_SAVED     rr_intervals.intervals[rr_intervals.index]
#define CURR          (&working_liebnitz)
#define RRI_CURR      CURR->rr_interval
#define STIM_CURR     CURR->stimuli
#define NOM_STIM_CURR CURR->nom_num_stims
#define G_VAL_CURR    CURR->g_val
#define SI_CURR       CURR->scan_index
#define RR_AVG_CURR   CURR->rr_avg
#define G_2BIG_CURR   CURR->g_too_big_count
#define G_2SMALL_CURR CURR->g_too_small_count
#define RRT_CURR      CURR->rr_target
#define DG_CURR       CURR->delta_g
#define STIM_ON_CURR  CURR->stim_on
#define N_AVG_CURR    CURR->num_rr_avg
#define G_ADJ_CURR    CURR->g_adjustment_mode

/*---------------------------------------------------------------------------- 
  Some initial values or otherwise private constants...                 
-----------------------------------------------------------------------------*/
static const   int INIT_NOM_NUM_STIMS = 20;  /* the nominal num_stims value  */
static const   int STIM_PERIOD        = 5;   /* in milliseconds              */
static const   int STIM_SUSTAIN       = 2;   /* in milliseconds              */
static const float STIM_VOLTAGE       = 1.0; /* Preferred stim voltage       */
static const float INITIAL_DELTA_G    = 0.1; /* for MCShared init.           */
static const   int INITIAL_NUM_RR_AVG = 10;  /* "   "         "              */
static const float INITIAL_G_VAL      = 1.0; /* "   "         "              */
static const   int G_MOD_THRESHOLD    = 2;   /* G_INC_CURR, C_DEC_CURR,      */
                                             /* and DG_CURR related to this  */
/*---------------------------------------------------------------------------*/

/* NB: This module needs at least a 1000 hz sampling rate!
   It will fail if that is not the case at module initialization, 
   and may produce undefined results if that is not the case while the 
   module is running. */
static const int REQUIRED_SAMPLING_RATE = 1000;

struct StimState { 
  int stims;
  int sustain;
  int waiting;
};

struct StimState state = {0, -1, -1};

static struct proc_dir_entry *proc_ent = 0;

int map_control_init (void)
{
  int retval = 0;

  if (rtp_shm->sampling_rate_hz < REQUIRED_SAMPLING_RATE) {
    printk("map_control: cannot start the module because the sampling rate of "
           "rt_process is not %d hz! map_control.o *requires* a %d hz period "
           "on the rt loop for its own internal simplicity.  The current "
           "rate that rt_process is looping at is: %d", 
           REQUIRED_SAMPLING_RATE, REQUIRED_SAMPLING_RATE, 
           (int)rtp_shm->sampling_rate_hz);
    return -ETIME;
  }

  if ( (retval = rtp_register_function(do_map_control_stuff))
       || (retval = init_shared_mem())
       || (retval = rtp_find_free_rtf(&shm->fifo_minor, MC_FIFO_SZ))
       || (retval = init_ao_chan()) 
       || (retval = rtp_activate_function(do_map_control_stuff))
    ) 
    map_control_cleanup();  

  proc_ent = create_proc_entry("map_control", S_IFREG|S_IRUGO, rtlab_proc_root);
  if (proc_ent)  /* if proc_ent is zero, we silently ignore... */
    proc_ent->read_proc = map_control_proc_read;
  
  
  return retval;
}

void map_control_cleanup (void)
{
  if (proc_ent)
    remove_proc_entry("map_control", rtlab_proc_root);

  rtp_deactivate_function(do_map_control_stuff);
  rtp_unregister_function(do_map_control_stuff);
  if (shm && shm->fifo_minor >= 0)  rtf_destroy(shm->fifo_minor);
  if (shm && shm->ao_chan >= 0)
    /* indicate that now this channel is free */
    _set_bit(shm->ao_chan, rtp_shm->ao_chans_in_use, 0);

  if (shm)  { rtos_shm_detach(shm); shm = 0; }
}

static void do_map_control_stuff (MultiSampleStruct * m)
{
  int have_spike;

  do_pre_control_stuff();

  /* is there a spike? */
  if ( (have_spike = 
        shm->spike_channel > -1 
        && _test_bit((int)shm->spike_channel, spike_info.spikes_this_scan))) { 

    calc_rr(); /* calculate the rr */
   
    calc_stim(); /* calculate the number of stims */

    calc_g(); 

    out_to_fifo();    

  }

  /* Note that stimulate modulates the stimulation voltage
     the assumption is that rt_process.o is running at precisely 1000 hz! */
  stimulate(have_spike);
  m = 0; /* to avoid compiler errors... */
}

static int init_shared_mem(void)
{

  memset(&working_liebnitz, 0, sizeof(MCLiebnitz));

  shm = 
    (MCShared *) rtos_shm_attach (MC_SHM_NAME,
                                   sizeof(MCShared));
  if (! shm)  return -ENOMEM;

  memset(shm, 0, sizeof(MCShared));

  rr_intervals.index = -1; 
  rr_intervals.n_intervals = 0;
  shm->g_val = G_VAL_CURR = INITIAL_G_VAL;
  shm->fifo_minor = -1;
  shm->spike_channel = -1;
  shm->delta_g = INITIAL_DELTA_G;
  shm->num_rr_avg = INITIAL_NUM_RR_AVG;
  shm->nom_num_stims = INIT_NOM_NUM_STIMS;
  
  if ( (shm->ao_chan = 
        find_free_chan(rtp_shm->ao_chans_in_use, rtp_shm->n_ao_chans)) < 0) 
     return -EBUSY; /* no ao chans found */

  shm->magic = MC_SHM_MAGIC;

  return 0;
}

/* probes shm->ao_chan to find the maximal possible output voltage and
   saves range setting in the static global ao_range */
static int init_ao_chan(void) 
{
  int n_ranges 
    = comedi_get_n_ranges(rtp_comedi_ao_dev_handle, rtp_shm->ao_subdev, 
                          shm->ao_chan);
  int max_v = 0, i;
  comedi_krange krange;

  /* sanity-check... */
  if (_test_bit(shm->ao_chan, rtp_shm->ao_chans_in_use)) {
    /* the user-requested ao_chan is no good as it's taken.. */
    shm->ao_chan = last_ao_chan_used;
    return -EBUSY;
  }
    

  /* indicate that now this channel is used */
  _set_bit(shm->ao_chan, rtp_shm->ao_chans_in_use, 1);

  if (last_ao_chan_used > -1 && last_ao_chan_used != shm->ao_chan)  
    /* indicate that now this channel is free */
    _set_bit(last_ao_chan_used, rtp_shm->ao_chans_in_use, 0);
 
  for (i = 0, ao_range = 0; i < n_ranges; i++) {
    comedi_get_krange(rtp_comedi_ao_dev_handle, rtp_shm->ao_subdev, 
                      shm->ao_chan, i, &krange);
    if (krange.max > max_v) {
      ao_range = i;
      ao_stim_level = comedi_get_maxdata(rtp_comedi_ao_dev_handle, 
                                         rtp_shm->ao_subdev, 
                                         shm->ao_chan);
      max_v = krange.max;
    }
  }
  last_ao_chan_used = shm->ao_chan;

  return 0;
}

/* core helper functions... */
static void calc_rr()
{
    NEXT_SAVED;
    
    /* save rr interval in milliseconds */
    RRI_CURR = RRI_SAVED = round(spike_info.period[shm->spike_channel]);   
}

static void calc_stim(void)
{
    int sum = 0,
        index = rr_intervals.index,
        factor = 0,
        i;
    double g = G_VAL_CURR;

    /* there's no point in computing these if we aren't 
       doing any sort of control, so return early if
       both stimulation is simulated AND g is being adjusted manually
    if (G_ADJ_CURR == MC_G_ADJ_MANUAL && !STIM_ON_CURR) return;
    */
    for (i = 0; i < N_AVG_CURR; i++) {
      sum += rr_intervals.intervals[index] * (N_AVG_CURR - i);
      factor += N_AVG_CURR - i;
      index = rri_prev_index();
    }

    /* factor should never be zero, but let's program defensively
       in case they set shm->num_rr_avg to 0  */
    RR_AVG_CURR = ( factor != 0 ? sum / factor : shm->rr_target);   
    
    STIM_CURR = NOM_STIM_CURR  +  (shm->rr_target - RR_AVG_CURR) * g;

    if (STIM_CURR < MC_MIN_NUM_STIMS) STIM_CURR = MC_MIN_NUM_STIMS;

    if (STIM_CURR > MC_MAX_NUM_STIMS) STIM_CURR = MC_MAX_NUM_STIMS;

}

static void stimulate(int new_spike)
{ 
  if (new_spike) {
    state.stims = STIM_CURR;
    state.waiting = 0;    
  } 

  /* if stimulation was shut off, reset our state to idle */
  if (!STIM_ON_CURR)  {state.sustain = 0; state.stims = 0; state.waiting = 0;}
  
  
  if ( state.stims && state.waiting == 0 ) {
    /* polarization (begin stimulation) */
    comedi_data_write(rtp_comedi_ao_dev_handle, 
                      rtp_shm->ao_subdev, 
                      shm->ao_chan, 
                      ao_range, 
                      AREF_GROUND,
                      ao_stim_level);
    state.stims--;
    state.waiting = STIM_PERIOD;
    state.sustain = STIM_SUSTAIN;
  }

  if ( state.sustain == 0 ) {
    /* decay and rest */
    comedi_data_write(rtp_comedi_ao_dev_handle, // reset
                      rtp_shm->ao_subdev, 
                      shm->ao_chan, 
                      ao_range, 
                      AREF_GROUND,
                      ao_stim_level / 2);
    
  }

  if (state.waiting >= 0)  state.waiting--;
  if (state.sustain >= 0)  state.sustain--;

}

static void calc_g(void)
{
  const int rr_diff = RRT_CURR - RR_AVG_CURR;

  if (G_ADJ_CURR != MC_G_ADJ_AUTOMATIC) return;

  if ( rr_diff > 0 ) {
    /* we need to decrease g */
    G_2SMALL_CURR++;
    G_2BIG_CURR = 0;
  } else if ( rr_diff < 0 ) {
    /* we need to increase g */
    G_2SMALL_CURR = 0;
    G_2BIG_CURR++;
  }
  /* is our threshold reached? 
     if our G_DEC_COUNT threshold is reached, we decrease G by DG_CURR, 
     if out G_INC_COUNT threshold is reached, we increase G by DG_CURR */
  G_VAL_CURR -= DG_CURR * (G_2BIG_CURR   >= G_MOD_THRESHOLD);
  G_VAL_CURR += DG_CURR * (G_2SMALL_CURR >= G_MOD_THRESHOLD);
  if (G_VAL_CURR < 0.0) G_VAL_CURR = 0.0; /* g cannot be less than 0 */
  shm->g_val = G_VAL_CURR;
}

static void out_to_fifo(void)
{
  rtf_put(shm->fifo_minor, CURR, sizeof(*CURR));
}

static int find_free_chan(volatile char *chans, unsigned int n_chans)
{
  unsigned int i;

  for (i = 0; i < n_chans; i++) 
    if (!_test_bit(i, chans)) return (int)i;
  return -1; /* nothing found */
}


static void do_pre_control_stuff(void)
{
  if (last_ao_chan_used != shm->ao_chan) 
    init_ao_chan();
 
  /* 
     safety/constraint checks 
  */
  if (shm->delta_g < MC_DELTA_G_MIN) shm->delta_g = MC_DELTA_G_MIN;
  else if (shm->delta_g > MC_DELTA_G_MAX) shm->delta_g = MC_DELTA_G_MAX;

  if (shm->num_rr_avg < MC_NUM_RR_AVG_MIN) shm->num_rr_avg = MC_NUM_RR_AVG_MIN;
  else if (shm->num_rr_avg > MC_NUM_RR_AVG_MAX) shm->num_rr_avg = MC_NUM_RR_AVG_MAX;

  if (shm->nom_num_stims > MC_MAX_NUM_STIMS) shm->nom_num_stims = MC_MAX_NUM_STIMS;
  else if (shm->nom_num_stims < MC_MIN_NUM_STIMS) shm->nom_num_stims = MC_MIN_NUM_STIMS;
    
  G_VAL_CURR = shm->g_val;
  SI_CURR = rtp_shm->scan_index;
  RRT_CURR = shm->rr_target;
  DG_CURR = shm->delta_g;
  N_AVG_CURR = shm->num_rr_avg;
  G_ADJ_CURR = shm->g_adjustment_mode;
  STIM_ON_CURR = shm->stim_on;
  NOM_STIM_CURR = shm->nom_num_stims;
  /* will be set later...
     RRI_CURR = 
     STIM_CURR = 
     GBIG_CURR = 
     GSMALL_CURR =
     RR_AVG_CURR = 
  */
  
  
}

/*-----------------------------------------------------------------------------
   Inline function definitions related to our private structs declared
   at the top of this file 
-----------------------------------------------------------------------------*/
static inline void rri_next(void) { rr_intervals.index = rri_next_index(); }
static inline void rri_prev(void) { rr_intervals.index = rri_prev_index(); }

static inline int rri_next_index(void) 
{ 
  return  (rr_intervals.index < 0 ? 0 : (rr_intervals.index + 1) % MC_N_SAVED_VALS);
}

static inline int rri_prev_index(void)
{ 
  if (rr_intervals.index < 0) return 0;
  return ( rr_intervals.index != 0 ? rr_intervals.index - 1 
           : MC_N_SAVED_VALS - 1 ); 
}

static inline int rri_rel_index(int offset)
{ 
  int saved_index = rr_intervals.index;

  if (offset < 0)
    while(offset++ < 0) rri_next();
  else
    while(offset-- > 0) rri_prev();

  saved_index ^= rr_intervals.index; /* swap using XOR trick */
  rr_intervals.index ^= saved_index;
  saved_index ^= rr_intervals.index;
  return saved_index;
}


static int map_control_proc_read (char *page, char **start, off_t off, int count, 
                               int *eof,  void *data)
{
  char g_val[23], delta_g[23], g_adj;
  PROC_PRINT_VARS;

  float2string(g_val, 23, G_VAL_CURR, 5);
  float2string(delta_g, 23, DG_CURR, 5);
  g_adj = G_ADJ_CURR;

  PROC_PRINT("MC Stimulation Experiment Realtime Kernel Module Statistics\n"
             "------------------------------------------------------------\n"
             "FIFO Minor Device: %d\n"
             "AI Channel ID to Monitor: %d\n"
             "AO Output (Stimulation) Channel ID: %d\n\n"
             "G Value: %s\n"
             "Delta-G: %s\n"             
             "G Adjustment Mode: %s\n"
             "Stim: %s\n"
             "Last Number of Stims Delivered: %d\n"
             "Nominal Number of Stimulations: %d\n"
             "Last RR Interval (in milliseconds): %d\n"
             "Average RR Interval (in milliseconds): %d\n"
             "Target RR Interval (in milliseconds): %d\n"
             "Number of Beats in RR Average: %d\n"
             "G-Too-Small-Count: %d\n"
             "G-Too-Big-Count: %d\n",
             shm->fifo_minor, shm->spike_channel, shm->ao_chan,             
             g_val, delta_g, 
             (g_adj == MC_G_ADJ_MANUAL ? "Manual" :
               (g_adj == MC_G_ADJ_AUTOMATIC ? "Automatic" : "Unknown")),
             (STIM_ON_CURR ? "Stim. Is ON" : "Stim. Is Off"),
             STIM_CURR,
             NOM_STIM_CURR,
             RRI_CURR,
             RR_AVG_CURR,
             shm->rr_target,
             N_AVG_CURR,
             G_2SMALL_CURR,
             G_2BIG_CURR
            );

  PROC_PRINT_DONE;
}



