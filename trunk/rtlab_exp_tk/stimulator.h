/*
 * This file is part of the RT-Linux Multichannel Data Acquisition System
 *
 * Copyright (C) 1999,2000 David Christini
 * Copyright (C) 2001,2002 Calin Culianu, David Christini
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
#ifndef _STIMULATOR_H
#include "rt_process.h"
#include "rtlab_cmd.h"


#define RTLAB_STIM_CONTINUOUS (-1) /* used as value for 
                                      struct rtlab_stim_params::num_trains
                                      to indicate continuous stimulation */
struct rtlab_stim_params
{
  double on_voltage; /* what voltage to use for the 'on' part -- the pulse */
  double off_voltage; /* what voltage for the off phase (rest) */
  int when_ms;        /* when in future to start stim. (0 == now) */
  int duration_ms;    /* the duration of each stim's 'on' pulse */
  int spacing_ms;  /* the duraction of each stim's 'off' phase.. ('on' phase
                      + 'off' phase == 1 pulse.. there are num_per_train stims
                      in a pulse  train.. see num_per_train below */
  int end_silence_ms; /* the amount of 'silence' to add after each pulse
                         train ends */
  int num_per_train; /* the number of stims to send per pulse train */
  int num_trains;    /* the number of pulse trains to send.. all pulse trains
                        follow each other perfectly
                        negative value means continuous 
                        (stim train never ends)
                        Use RTLAB_STIM_CONTINUOUS here... */
};

#define EMPTY_STIM_PARAMS ((struct rtlab_stim_params){0,0,0,0,0,0,0,0})
#define CLEAR_STIM_PARAMS(x) (memset(x, 0, sizeof(struct rtlab_stim_params)))


#ifdef RTLAB_INTERNAL
/* called from rtlab.o upon module initialization */
extern int init_stim_engine(void);
extern void cleanup_stim_engine(void);
#endif


/* Opaque type */
struct rtlab_stimulator;

#define DEFAULT_STIMUALTOR_NUM_PER_TRAIN (100)
/* this is a pre-allocated stimulator with max_num_per_train set to 100 */
extern struct rtlab_stimulator * rtlab_default_stimulator;
#define DEFAULT_STIMULATOR rtlab_default_stimulator

/* Call this from NON-Realtime context: creates an rtlab_stimulator
   object, which is to be used for all outside interactions
   with this module */
extern struct rtlab_stimulator *
rtlab_stim_alloc(const struct rtlab_comedi_context *, 
                 unsigned int max_num_per_train);

/* Call this from non-realtime only */
extern void rtlab_stim_free(struct rtlab_stimulator *);

/* 
   Safe to call from realtime -- reassigns a new rtlab_comedi_context
   to a given rtlab_stimulator handle 
   Returns 0 on success, ENOENT if the stimulator is invalid, 
   or if the context is invalid EINVAL;
*/
extern int rtlab_stim_set_context(struct rtlab_stimulator *, 
                                  const struct rtlab_comedi_context *);
/* 
   A callback is assigned to this stimulator..
   If the stimulator is processing a continuous stim, the callback
   is called whenever one stim cycle ends.
   If the stimulator is finite (non-continuous) the callback is called
   once all the stim trains are finished. 

   Use NULL for the callback to clear an already-assigned callback.

   Returns 0 on success, EINVAL if the stimulator is not a valid
   stimulator.
*/
extern int rtlab_stim_set_callback(struct rtlab_stimulator *,
                                   rtlab_cmd_callback_t *callback,
                                   void *callback_arg);

/* 
   frontend to rtlab_cmd - a stimulator, with (optional) continuous support 
   
   Return values: 
                    0 Indicates success for a oneshot (non-continnuous) stim
                  > 0 (positive) CONTINUOUS STIMS ONLY!
                      a 'handle' that is used to cancel/restart a continuous 
                      stimulation
                  < 0 (negative) Indicates error. The absolute value of this
                      number is the exact error code from asm/errno.h

  Note: Do not try and start more than RTLAB_STIM_MAX_SIMULTANEOUS 
        continuous stims at once, as you will get an error return
        value.
       
*/
extern int  rtlab_stimulate  ( struct rtlab_stimulator *,
                               const struct rtlab_stim_params * );
/* 
   Cancels a (continuous only!) stim.  The parameter is an int
   that was returned from a previously created continuous (num=-1) stim
   from rtlab_stimulate()
*/
extern int  rtlab_cancel_stim ( struct rtlab_stimulator * );
#endif
