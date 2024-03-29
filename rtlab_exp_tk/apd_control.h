/*! APD Control - The kernel side defs.. */

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
#ifndef _APD_CONTROL_H
#  define _APD_CONTROL_H

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

  /* some of these could be inferred from the structs in shared memory 
     but is copied to the MCSnapShot structs for historical purposes */
  scan_index_t scan_index; /* current scan index */
  int pi; /* the pacing interval in ms for this beat */
  int delta_pi; /* the perturbation to pacing interval in ms */
  int apd_channel; /* the channel with the apd's that the algorithm are trying to control */
  int apd_xx; 
  int apd; /* computed current apd */
  int di; /* current di */
  double v_apa; /* Action Potential Amplitude (APA) in volts */
  double v_baseline; /* baseline voltage */
  scan_index_t ap_ti; /* AP initial time (when crossed threshold in upward direction) */
  scan_index_t ap_tf; /* AP end time (when crossed below APDxx voltage value) */
  int consec_alternating; /* number of perturbations in a row that alternated on, off, on, off */
  double g_val; /* the value of our magic coefficient g */
  int ao_chan;

  /* all below fields are here as a record of what the ui had told the
     realtime process to do during this scan -- the actual
     communication of the ui to the rt process happens via struct MCShared */
  char pacing_on; /* ui-controlled */
  int nominal_pi; /* ui-controlled */
  char control_on;    /* ui-controlled */
  char continue_underlying;    /* ui-controlled */
  char only_negative_perturbations;  /* ui-controlled */
  char target_shorter;   /* ui-controlled */
  char g_adjustment_mode; /* ui-controlled */
  float delta_g; /* ui-controlled */
  int link_to_ao0; /* whether (1) or not (0) AO1 control is linked to AO0 pacing */
  int ao0_ao1_cond_time; /* time it takes to conduct from AO0 electrode to AO1 electrode*/
};

#ifndef __cplusplus
  typedef struct MCSnapShot MCSnapShot;
#endif

/* Two defines for MCShared.g_adjustment_mode:
   Automatic means the control loop is always re-adjusting g
   Manual means the UI is only place g gets modified */
#define MC_G_ADJ_MANUAL 0x0
#define MC_G_ADJ_AUTOMATIC 0x1

#define NumAPDs 8 /*number of APD channels*/
#define NumAOchannels 2 /*number of AO channels ... assumes they are numbered 0 and 1*/

//shared memory
struct MCShared {
  int ai_chan_this_ao_chan_is_dependent_on[NumAOchannels]; /* The channel a given AO chan is monitoring */
  double apd_xx; // xx=0.1 for APD90, 0.2 for APD80, etc.
  int ao_chan; /* obsolete, but may still remain in apd_control.c */
  char pacing_on[NumAOchannels];    /* if nonzero, pace */
  int link_to_ao0; /* whether (1) or not (0) AO1 control is linked to AO0 pacing */
  int ao0_ao1_cond_time; /* time it takes to conduct from AO0 electrode to AO1 electrode*/
  int nominal_pi[NumAOchannels]; /* in milliseconds */  
  char control_on[NumAOchannels];    /* if nonzero, control */
  char continue_underlying[NumAOchannels];    /* if nonzero, continue underlying pacing during control */
  char only_negative_perturbations[NumAOchannels];    /* if nonzero, then allow only negative perturbations, otherwise both + and - */
  char target_shorter[NumAOchannels];    /* if nonzero, target shorter APD for first control attempt */
  char g_adjustment_mode[NumAOchannels]; /* see above #defines */

  int fifo_minor; /* Just informational for user side to know where the fifo is */

  volatile double g_val[NumAOchannels];  /* the ui can change this and it affects control */
  volatile float delta_g[NumAOchannels]; /* the amount we modify g each time we do control */
  unsigned int rt_monitoring_frequency; /* This is the frequency that the 
                                           realtime side is running at, 
                                           in Hz */
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
#define MC_DELTA_G_MIN ((float)0.001)
#define MC_DELTA_G_MAX ((float)0.5)
#define MC_DELTA_G_FIXEDPOINT_MULT (1e6)
/* The below don't affect kernel whatsoever, just GUI */
#define MC_DELTA_G_SCROLLBAR_STEP (mc_delta_g_toint(MC_DELTA_G_MIN / MC_DELTA_G_MAX))
/* below affects GUI display of delta-g value.. this becomes the '*' in
   an sprintf %.*f */
#define MC_DELTA_G_GUI_PRECISION (3)

static inline   int mc_delta_g_toint(float x) 
                { return (int)(x * (int)MC_DELTA_G_FIXEDPOINT_MULT); }
static inline float mc_delta_g_fromint(int x) 
                { return x / MC_DELTA_G_FIXEDPOINT_MULT; }

#ifdef __cplusplus
}
#endif

#endif
