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

#include <linux/comedilib.h>

#include <mbuff.h>
#include <rtl_fifo.h>
#include <rtl_sched.h>

#include "rt_process.h"
#include "avn_stim.h"

/*----------------------------------------------------------------------------
  Some Private Structures...
-----------------------------------------------------------------------------*/
#define AVN_N_SAVED_VALS 100
#define AVN_STARTING_G_VAL 5.0

struct RRIntervals {
  int intervals[AVN_N_SAVED_VALS]; /* in milliseconds */
  int index; /* index of last value in above array */
  int wrapped; /* true iff index has wrappped past AVN_N_SAVED_VALS */ 
  scan_index_t last_scan_index;
};

typedef struct RRIntervals RRIntervals;

struct GVals {
  double vals[AVN_N_SAVED_VALS];
  int index; /* index of last value in above array */
  int wrapped; /* true iff index has wrappped past AVN_N_SAVED_VALS */ 
  scan_index_t last_scan_index;
};

typedef struct GVals GVals;

struct Stimuli {
  int stimuli[AVN_N_SAVED_VALS];
  int index; /* index of last value in above array */
  int wrapped; /* true iff index has wrappped past AVN_N_SAVED_VALS */ 
  scan_index_t last_scan_index;
};

typedef struct Stimuli Stimuli;

/*  Some inline 'macros' related to the above structs */
inline int avn_next_index(int index, int *w_flg);
inline int avn_prev_index(int index, const int *w_flg);
inline int avn_rel_index(int index, int offset, const int *w_flg);
/*---------------------------------------------------------------------------*/

#define MODULE_NAME "AVN Stim"

MODULE_AUTHOR("David J. Christini, PhD and Calin A. Culianu [not PhD :(]");
MODULE_DESCRIPTION(MODULE_NAME ": A Real-Time stimulation and control add-on for daq system and rt_process.o.");

int avn_stim_init(void); 
void avn_stim_cleanup(void);

static int init_shared_mem(void);
static int init_fifo(int *, int);
static int init_ao_chan(void);

static void do_avn_control_stuff(MultiSampleStruct *);

/* 'inner' helper functions... */
static void do_pre_control_stuff(void);
static void calc_rr(void);  
static void calc_stim(void); 
static void stimulate(int new_stim); /* if true, a new stim is started */
static void calc_g(void);
static void out_to_fifo(void);
static int find_free_chan(char *, unsigned int);

module_init(avn_stim_init);
module_exit(avn_stim_cleanup);

static AVNShared *shm = 0;

static RRIntervals  rr_intervals;
static Stimuli      stimuli;
static GVals        gvals;
static int          last_ao_chan_used = -1;
static unsigned int ao_range = 0;
static lsampl_t     ao_stim_level = 0; /* basically comedi_get_maxdata()- 1 */

inline int *      _RRI_CURR (void) 
               { return &rr_intervals.intervals[rr_intervals.index];}
inline double *    _GV_CURR  (void)         
               { return &gvals.vals[gvals.index];                   }
inline int *       _STIM_CURR(void)
               { return &stimuli.stimuli[stimuli.index];            }

#define RRI_CURR  *_RRI_CURR()
#define GV_CURR   *_GV_CURR()
#define STIM_CURR *_STIM_CURR()
static const int MAX_STIMS = 25; /* the maximum number of stims per spike */
static const int STIM_PERIOD = 5; /* in milliseconds */
static const int STIM_SUSTAIN = 1; /* in milliseconds */

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

int avn_stim_init (void)
{
  int retval = 0;


  if (rtp_shm->sampling_rate_hz < REQUIRED_SAMPLING_RATE) {
    printk("avn_stim: cannot start the module because the sampling rate of "
           "rt_process is not %d hz! avn_stim.o *requires* a %d hz period "
           "on the rt loop for its own internal simplicity.  The current "
           "rate that rt_process is looping at is: %d", 
           REQUIRED_SAMPLING_RATE, REQUIRED_SAMPLING_RATE, 
           (int)rtp_shm->sampling_rate_hz);
    return -ETIME;
  }

  if ( (retval = rtp_register_function(do_avn_control_stuff))
       || (retval = init_shared_mem())
       || (retval = init_fifo(&shm->in_fifo_minor, AVN_USER_KERNEL_FIFO_SZ))
       || (retval = init_fifo(&shm->out_fifo_minor, AVN_KERNEL_USER_FIFO_SZ))
       || (retval = init_ao_chan()) 
       || (retval = rtp_activate_function(do_avn_control_stuff))
    ) 
    avn_stim_cleanup();  

  return retval;
}

void avn_stim_cleanup (void)
{
  rtp_deactivate_function(do_avn_control_stuff);
  rtp_unregister_function(do_avn_control_stuff);
  if (shm && shm->in_fifo_minor >= 0)  rtf_destroy(shm->in_fifo_minor);
  if (shm && shm->out_fifo_minor >= 0) rtf_destroy(shm->out_fifo_minor);
  if (shm && shm->ao_chan >= 0)
    /* indicate that now this channel is free */
    _set_bit(shm->ao_chan, rtp_shm->ao_chans_in_use, 0);

  if (shm)  { mbuff_free(AVN_SHM_NAME, shm); shm = 0; }
}

static void do_avn_control_stuff (MultiSampleStruct * m)
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
  if (shm->stim_on) stimulate(have_spike);
  m = 0; /* to avoid compiler errors... */
}

static int init_shared_mem(void)
{
  shm = 
    (AVNShared *) mbuff_alloc (AVN_SHM_NAME,
                               sizeof(AVNShared));
  if (! shm)  return -ENOMEM;

  memset(shm, 0, sizeof(AVNShared));

  rr_intervals.index = -1; 
  stimuli.index = -1;
  gvals.index = 0; 
  shm->latest_snapshot.g_val = gvals.vals[0] = AVN_STARTING_G_VAL;
  shm->out_fifo_minor = shm->in_fifo_minor = -1;
  shm->spike_channel = -1;

  if ( (shm->ao_chan = 
        find_free_chan(rtp_shm->ao_chans_in_use, rtp_shm->n_ao_chans)) < 0) 
     return -EBUSY; /* no ao chans found */

  shm->magic = AVN_SHM_MAGIC;

  return 0;
}


static int init_fifo(int *fifo, int size)
{
  unsigned int i;

  /* keep trying to create a fifo until we find a free one */
  for (i = 0; i < RTF_NO; i++) {
    int ret;

    if (i == rtp_shm->ai_fifo_minor 
        || i == rtp_shm->ao_fifo_minor) continue;

    ret = rtf_create(i, size);

    /* the rtf_create docs contradict the actual code for rtf_create... */
    if (ret == size || ret == 0) {
      /* got it! */      
      *fifo = i;
      printk(MODULE_NAME ": init_fifo(): fifo minor is %u\n", *fifo);
      return 0;
    } else if (ret != -EBUSY) return ret;          
  }

  return -EBUSY;
}

/* probes shm->ao_chan to find the maximal possible output voltage and
   saves range setting in the static global ao_range */
static int init_ao_chan(void) 
{
  int n_ranges 
    = comedi_get_n_ranges(rtp_shm->ao_minor, rtp_shm->ao_subdev, shm->ao_chan);
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

  ao_range = 0;

  for (i = 0; i < n_ranges; i++) {
    comedi_get_krange(rtp_shm->ao_minor, rtp_shm->ao_subdev, shm->ao_chan, 
                      i, &krange);
    if (krange.max > max_v) {
      ao_range = i;
      ao_stim_level = comedi_get_maxdata(rtp_shm->ao_minor, 
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
    /* calculate the rr interval for this spike */
    rr_intervals.index = avn_next_index(rr_intervals.index, 
                                        &rr_intervals.wrapped);

    /* save rr interval in milliseconds */
    RRI_CURR = round(spike_info.period[shm->spike_channel]);
   
    rr_intervals.last_scan_index = rtp_shm->scan_index;
}

static void calc_stim(void)
{
    int rr_n = 0,
        sum = 0,
        num = (rr_intervals.wrapped 
               ? AVN_N_SAVED_VALS 
               : rr_intervals.index + 1),
        index = rr_intervals.index,
        i;
    double g = GV_CURR;
    static const double very_small_g = 0.0000001;

    stimuli.index = avn_next_index(stimuli.index, 
                                   &stimuli.wrapped);

    for (i = 0; i < num; i++) {
      sum += rr_intervals.intervals[index];
      index = avn_prev_index(index, &rr_intervals.wrapped);
    }
    
    rr_n = ( num != 0 ? sum / num : shm->rr_target);   
    
    if (g == 0.0) g = very_small_g; /* prevent div by 0 */

    STIM_CURR = (rr_n - shm->rr_target) / g;

    if (STIM_CURR < 0) STIM_CURR = -STIM_CURR;

    if (STIM_CURR > MAX_STIMS) STIM_CURR = MAX_STIMS;

    stimuli.last_scan_index = rtp_shm->scan_index;
}

static void stimulate(int new_spike)
{ 
  if (new_spike) {
    state.stims = STIM_CURR;
    state.waiting = 0;    
  } 
  
  if ( state.stims && state.waiting == 0 ) {
    /* polarization (begin stimulation) */
    comedi_data_write(rtp_shm->ao_minor, 
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
    comedi_data_write(rtp_shm->ao_minor, // reset
                      rtp_shm->ao_subdev, 
                      shm->ao_chan, 
                      ao_range, 
                      AREF_GROUND,
                      ao_stim_level / 2);
    
  }

  if (state.waiting >= 0)  state.waiting--;
  if (state.sustain >= 0)  state.sustain--;

}

static double get_g_from_user(int *ok) 
{
  double g = GV_CURR, last_g = g;
  int n_read = 0;

  if (ok) *ok = 0;

  while ( (n_read = rtf_get(shm->in_fifo_minor, (char *)&g, sizeof(g))) > 0 ) {
    if (n_read != sizeof(g)) g = last_g; else last_g = g;
    if (ok) *ok = 1;
  }

  return g;
}

static void calc_g(void)
{
    if (shm->g_adjustment_mode == AVN_G_ADJ_AUTOMATIC) {
      /* ask dave about this!! */
      gvals.last_scan_index = rtp_shm->scan_index;
    }
}

static void out_to_fifo(void)
{
  AVNLiebnitz out = { RRI_CURR, STIM_CURR, GV_CURR, rtp_shm->scan_index };

  rtf_put(shm->out_fifo_minor, &out, sizeof(out));
  memcpy(&shm->latest_snapshot, &out, sizeof(out));
}

static int find_free_chan(char *chans, unsigned int n_chans)
{
  unsigned int i;

  for (i = 0; i < n_chans; i++) 
    if (!_test_bit(i, chans)) return (int)i;
  return -1; /* nothing found */
}


static void do_pre_control_stuff(void)
{
  int user_changed_g = 0;
  double new_g;

  if (last_ao_chan_used != shm->ao_chan) 
    init_ao_chan();
 
  new_g = get_g_from_user(&user_changed_g);

  if (user_changed_g) {
    gvals.index = avn_next_index(gvals.index, &gvals.wrapped);
    GV_CURR = new_g;
  }
    
}

/*-----------------------------------------------------------------------------
   Inline function definitions related to our private structs declared
   at the top of this file 
-----------------------------------------------------------------------------*/
inline int avn_next_index(int index, int *w_flg) 
{ 
  int ret = (index < 0 ? 0 : (index + 1) % AVN_N_SAVED_VALS);

  /* set the wrapped flag if we wrapped around in our saved values */
  if (w_flg && index >= 0 && ret < index) *w_flg = 1;
  return ret;
}

inline int avn_prev_index(int index, const int *w_flg)
{ 
  if (w_flg && !*w_flg && index <= 0) return 0;
  return ( index > 0 ? index - 1 : AVN_N_SAVED_VALS - 1 ); 
}

inline int avn_rel_index(int index, int offset, const int *w_flg)
{ 
  int i = 0;

  if (offset < 0)
    for (i = offset; i > 0; i--) index = avn_prev_index(index, w_flg);
  else
    for (i = 0; i < offset && (!w_flg||(!*w_flg && index < AVN_N_SAVED_VALS));
         i++) index = avn_next_index(index, 0);

  return index;
}
