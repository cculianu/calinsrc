/*! Map Control - The kernel side defs.. */

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
#ifndef _MAP_CONTROL_H
#  define _MAP_CONTROL_H

#include "shared_stuff.h"

#ifdef __cplusplus
extern "C" {
#endif


/* A struct for use in the user-process-bound fifo, called Liebnitz since this
   is akin to the 'derivative' of a function.. namely a momentary analysis of
   state 

   A lot of these fields are duplicated from MCShared, but this is necessary
   so as to record the exact values of the variables in the experiment
   for each spike interval (since UI task might miss some spikes if it doesn't
   get scheduled by Linux).
*/
struct MCSnapShot {
  int rr_interval; /* the time in ms for this beat */
  int stimuli; /* the number of stims delivered */
  double g_val; /* the value of our magic coefficient g */
  /* below is stuff that can be inferred from the structs in shared memory 
     but is copied to the MCSnapShot structs for historical purposes */
  scan_index_t scan_index; /* copied from rt_process's rtp_shm */
  int rr_avg; /* computed momentary average rr, num_rr_avg plays into this */
  int g_too_small_count; /* computed */
  int g_too_big_count; /* computed */

  /* all below fields are here as a record of what the ui had told the
     realtime process to do during this rr interval -- the actual
     communication of the ui to the rt process happens via struct MCShared */
  int rr_target; /* ui-controlled */
  float delta_g; /* ui-controlled */
  int num_rr_avg; /* ui-controlled */
  int nom_num_stims; /* ui-controlled */
  char stim_on;    /* ui-controlled */
  char g_adjustment_mode; /* ui-controlled */
};

#ifndef __cplusplus
  typedef struct MCSnapShot MCSnapShot;
#endif


/* Two defines for MCShared.g_adjustment_mode:
   Automatic means the control loop is always re-adjusting g
   Manual means the UI is only place g gets modified */
#define MC_G_ADJ_MANUAL 0x0
#define MC_G_ADJ_AUTOMATIC 0x1

struct MCShared {
  int rr_target; /* in milliseconds */  
  volatile double g_val;  /* the ui can change this and it affects control */
  volatile float delta_g; /* the amount we modify g each time we do control */
  volatile int num_rr_avg; /* the number of recent beats we take into account 
                              to reach rr_avg (see MCSnapShot::rr_avg) */
  volatile int nom_num_stims; /* the number of stims that we +/- around in our
                                 control algorithm */
  char stim_on;    /* if nonzero, do the actual stimulations */
  char g_adjustment_mode; /* see above #defines */
  int fifo_minor; /* Just informational for user side to know where the fifo is */
  int spike_channel; /* The channel we are monitoring in the kernel */
  int ao_chan; /* The channel that is doing the control, specified by user interface */
  unsigned int magic;   /* should always equal MC_SHM_MAGIC */
  int reserved[4]; /* just so i can look like i know what i am doing... */
};

#ifndef __cplusplus
  typedef struct MCShared MCShared;
#endif

#define MC_SHM_NAME "MC Stim Shared Memory"
#define MC_SHM_MAGIC (0xf001edU)

#define MC_FIFO_SZ (sizeof(struct MCSnapShot) * 100)

/* Some sanity limits imposed by both UI and realtime thread */
#define MC_NUM_RR_AVG_MAX 50
#define MC_NUM_RR_AVG_MIN 1
#define MC_DELTA_G_MIN ((float)0.01)
#define MC_DELTA_G_MAX ((float)0.5)
#define MC_MAX_NUM_STIMS  150 /* maximum number stims in AO   */
#define MC_MIN_NUM_STIMS  0   /* obvious, but still set here  */

static inline   int mc_delta_g_toint(float x) { return (int)(x * 100); }
static inline float mc_delta_g_fromint(int x) { return x / 100.0; }

#ifdef __cplusplus
}
#endif

#endif
