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

#include "rt_process.h"
#include "rtos_middleman.h"
#include "map_control.h"

#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL"); 
#endif

#define MODULE_NAME "MC Stim"

MODULE_AUTHOR("David J. Christini, PhD and Calin A. Culianu [not PhD :(]");
MODULE_DESCRIPTION(MODULE_NAME ": A Real-Time stimulation and control add-on for daq system and rtlab.o.\n$Id$");

int map_control_init(void); 
void map_control_cleanup(void);

static int init_shared_mem(void);
static int init_ao_chan(void);

static void do_map_control_stuff(MultiSampleStruct *);

/* 'inner' helper functions... */
static void setup_analog_output(void);
static void do_pacing(void);  
static void calculate_apd_and_control_perturbation(MultiSampleStruct *);
static void do_control(void); 
static void automatically_adapt_g(void);
static void out_to_fifo(void);
static int find_free_chan(volatile char *, unsigned int);

module_init(map_control_init);
module_exit(map_control_cleanup);

static MCShared *shm = 0;  //pointer to the shared memory ... communicates between real-time
                                              // process and non-real-time GUI
static MCSnapShot  working_snapshot; /* used for inter-function comm.  btwn RT and non-RT GUI  */

static int          last_ao_chan_used = -1;
static unsigned int ao_range = 0;
static lsampl_t     ao_stim_level = 0, /* basically comedi_get_maxdata()- 1 */
                    ao_rest_level = 0; /* either ao_stim_level / 2 or 0 */

/*-------------------------------------------------------------------------- 
  Some initial parameters or otherwise private constants...                 
  Tweak these to suit your taste...
---------------------------------------------------------------------------*/
static const  int STIM_PULSE_WIDTH       = 2;   /* in milliseconds              */
static const float STIM_VOLTAGE       = 5.0; /* Preferred stim voltage       */
static const float INITIAL_DELTA_G    = 0.1; /* for MCShared init.           */
static const float INITIAL_G_VAL      = 1.0;    /* "   "         "           */
static const int MAX_PEAK_TIME_AFTER_THRESHOLD = 25; // max time to look for AP peak after
static const int INITIAL_PI=500;
static const float RESET_VMIN = 999.0;
static const float RESET_VMAX = -999.0;
/*---------------------------------------------------------------------------*/

/* NB: This module needs at least a 1000 hz sampling rate!
   It will fail if that is not the case at module initialization, 
   and may produce undefined results if that is not the case while the 
   module is running. */
// ***** this should be changes so sampling rate is not hard wired
static const int REQUIRED_SAMPLING_RATE = 1000;

//structure storing the state of the system so that one scan knows the
//state from the previous scan
struct StimState {    //look in map_control.h and map_control.cpp for comments on these variables
  int pacing_pulse_width_counter;
  int pacing_interval_counter;
  int find_peak;
  scan_index_t ap_ti;
  scan_index_t ap_t90;
  double vmin_since_last_detected_thresh_crossing;
  double vmin_previous;
  double vmin_previous_previous;
  double vmax_since_last_detected_thresh_crossing;
  double v90;
  int apd;
  int previous_apd;
  int di;
  int previous_di;
  int control_stimulus_called;
  int control_interval_counter;
  int control_pulse_width_counter;
  int scan_index;
  int consec_alternating;
  int pi;
  int delta_pi;
  int past_perturb_sign[4];
};

struct StimState state = {pacing_pulse_width_counter:0, pacing_interval_counter:0, find_peak:0, ap_ti:0, ap_t90:0, 
			  vmin_since_last_detected_thresh_crossing:999.0,   //should use RESET_VMIN !!!
			  vmin_previous:999.0, vmin_previous_previous:999.0,
			  vmax_since_last_detected_thresh_crossing:-999.0, //should use RESET_VMAX,
                                          v90:0, apd:0, previous_apd:0, di:0, previous_di:0,
		          control_stimulus_called:0, control_interval_counter:0, control_pulse_width_counter:-1,
		          scan_index:0, consec_alternating:0,
		          pi:0, delta_pi:0, 
		          past_perturb_sign:{1,0,1,0} /*init to alternating*/
};

/****************************************************************************************/
/* do_map_control_stuff is the main "brain" of the pacing and control system. 
   this function gets run once every analog acquisition scan ...
   so it, and its sub-functions, are  where most algorithmic changes will need 
    to be made. */
/****************************************************************************************/
static void do_map_control_stuff (MultiSampleStruct * m)
{

  /* Note that do_pacing assumes that rt_process.o is running at precisely 1000 hz! */
  setup_analog_output();

  do_pacing();

  calculate_apd_and_control_perturbation(m);

  if (state.control_stimulus_called) do_control();

  m = 0; /* to avoid compiler errors... ??? */
}

// this function does periodic pacing at the PI controlled by the user in the GUI
// via the shm->nominal_pi shared memory variable
static void do_pacing(void)
{ 

  if ( (state.pacing_interval_counter == 0) && (shm->pacing_on) ) { 
    /* polarization (begin stimulation) */
    comedi_data_write(rtp_comedi_ao_dev_handle, 
                      rtp_shm->ao_subdev, 
                      shm->ao_chan, 
                      ao_range, 
                      AREF_GROUND,
                      ao_stim_level);
    state.pacing_interval_counter = shm->nominal_pi;
    state.pacing_pulse_width_counter = STIM_PULSE_WIDTH;
    //state.last_stim_given = rtp_shm->scan_index;
  }

  if ( state.pacing_pulse_width_counter == 0 ) {
    /* decay and rest */
    comedi_data_write(rtp_comedi_ao_dev_handle, // reset
                      rtp_shm->ao_subdev, 
                      shm->ao_chan, 
                      ao_range, 
                      AREF_GROUND,
                      ao_rest_level);
    
  }

  if (state.pacing_interval_counter > 0)  state.pacing_interval_counter--;
  if (state.pacing_pulse_width_counter > 0)  state.pacing_pulse_width_counter--;

}

//self-explanatory function title
static void calculate_apd_and_control_perturbation(MultiSampleStruct * m)
{

  //voltage_at used MultiSampleStruct * m to return the voltage of the requested channel.
  double this_voltage=voltage_at(shm->map_channel,m);

  //look for vmin on every scan ... this may be overkill, but it's safest in case signal drifts
  //and we miss an AP (i.e., if we only looked during part of the AP cycle and we miss an AP
  //then we may stop looking for vmin)
  if (this_voltage < state.vmin_since_last_detected_thresh_crossing)
    state.vmin_since_last_detected_thresh_crossing = this_voltage;

  // is there a threshold crossing occuring at this scan?
  if ( (shm->map_channel > -1 && _test_bit((int)shm->map_channel, spike_info.spikes_this_scan))) {
    state.find_peak=1;                        //if so, set find_peak so we start looking for the peak
    state.ap_ti=rtp_shm->scan_index; //this scan is the beginning of the action potential    

    state.vmin_previous_previous = state.vmin_previous; //store
    state.vmin_previous = state.vmin_since_last_detected_thresh_crossing; //store
    state.vmin_since_last_detected_thresh_crossing = RESET_VMIN; //reset
    state.vmax_since_last_detected_thresh_crossing=RESET_VMAX; //reset
  }

  //check this scan to see if it is the peak AP voltage ... this search will be run for each scan
  //after threshold crossing occurred until MAX_PEAK_TIME_AFTER_THRESHOLD ms 
  //after the threshold  crossing occurred
  while (state.find_peak && state.find_peak<MAX_PEAK_TIME_AFTER_THRESHOLD) { 
    if (this_voltage > state.vmax_since_last_detected_thresh_crossing)
      state.vmax_since_last_detected_thresh_crossing = this_voltage; 
    state.find_peak++;
    //now that we've found peak voltage, compute the voltage for apd90
    if (state.find_peak==MAX_PEAK_TIME_AFTER_THRESHOLD) { 
      double which_min=state.vmin_previous;
      //**** this really should be using < which_min, but until the user can control APD90,80,70, etc.
      //**** this will have to do ... change this!!!
      if (state.vmin_previous_previous > which_min) which_min = state.vmin_previous_previous;
      state.v90 = 0.1*(state.vmax_since_last_detected_thresh_crossing - which_min)
	+ which_min;

      /* used to debug
	 shm->g_val = state.v90;
	 state.consec_alternating+=5; //TEMP: remove this !!!!!!
	 state.consec_alternating++; //TEMP: remove this !!!!!!
	 out_to_fifo();    // write snapshot to fifo at the end of every action potential
      */

    }
  }

  // is this scan the first time we cross back below apd90 ... if so, determine apd and do other
  // stuff, including compute control perturbation
  // obviously, this process can only occur after the peak voltage was found in the previous section
  if ((state.find_peak==MAX_PEAK_TIME_AFTER_THRESHOLD) && (this_voltage<state.v90)) {
    state.find_peak=0;
    //set previous_apd and previous_di before we make changes to new di and new apd
    state.previous_apd = state.apd;
    state.previous_di=state.di;
    //set new di before we change ap_t90 (because we need to use previous ap_t90)
    state.di=state.ap_ti-state.ap_t90; //this DI is time since last AP_t90;
    //for new apd
    state.ap_t90=rtp_shm->scan_index;
    state.apd = state.ap_t90-state.ap_ti;

    // compute control perturbation
    if (shm->control_on) { 

      //shift past perturbation values ... for automatic g adaptation
      state.past_perturb_sign[0]= state.past_perturb_sign[1];
      state.past_perturb_sign[1]= state.past_perturb_sign[2];
      state.past_perturb_sign[2]= state.past_perturb_sign[3];

      // ****** note minus sign inserted in  front of g_val!!!  djc: 9/25/02
      state.delta_pi = -(int)(shm->g_val*(state.previous_apd - state.apd));
      //only perturb if delta_pi negative and larger than stimulus duration
      //if (state.delta_pi<=(-1)) {
      if (1) {
	state.control_stimulus_called=1;
	// control stimulus must occur at the time expected for the next pacing stimulus
	// (which is given by state.pacing_interval_counter ... which at the current scan should be equal
	//   to nominal_pi - apd), plus the perturbation (which is negative).
	state.control_interval_counter = state.pacing_interval_counter+state.delta_pi;
	state.past_perturb_sign[3]=0;
      }
      else  state.past_perturb_sign[3]=1;

      if ((shm->g_adjustment_mode) == MC_G_ADJ_AUTOMATIC) automatically_adapt_g(); 

    }
    //write to fifo, which in map_control.cpp will be saved to disk
    out_to_fifo();    // write snapshot to fifo at the end of every action potential
                             // note that end of ap was chosen arbitrarily .. could have been beginning
                             // may make more sense to do it whenever a stimulus is delivered

  }  //end of   if ((state.find_peak==MAX_PEAK_TIME_AFTER_THRESHOLD) && ...

}

// this function performs automatic adaptation of g (control proportionality constant)
// according to the algorithm in Hall&Christini, PRE, vol. 63, article 06204, 2001
static void automatically_adapt_g(void)
{

  //if perturbation sign has not alternated 4 times in a row, then we are overperturbing,
  //meaning our g is too large, so we should decrease g.
  if ( (state.past_perturb_sign[3] == state.past_perturb_sign[2]) ||
       (state.past_perturb_sign[2] == state.past_perturb_sign[1]) ||
       (state.past_perturb_sign[1] == state.past_perturb_sign[0]) ) {
    shm->g_val -= shm->delta_g;   // decrease g if perturb sign not alternating
                                                            // i.e., decrease the size of perturbations
    state.consec_alternating = 0; 
  }
  else {
    shm->g_val += shm->delta_g; // else perturbation sign has alternated 4 times in  a row,
                                                      // meaning we are underperturbing, so we increase g
    state.consec_alternating = 4;
  }

  // don't let g be < 0
  if (shm->g_val < 0.0) shm->g_val = 0;

}

// this function does alternans control
static void do_control(void)
{ 

  if ( (state.control_interval_counter == 0) ) { 
    /* polarization (begin stimulation) */
    comedi_data_write(rtp_comedi_ao_dev_handle, 
                      rtp_shm->ao_subdev, 
                      shm->ao_chan, 
                      ao_range, 
                      AREF_GROUND,
                      ao_stim_level);
    state.control_pulse_width_counter = STIM_PULSE_WIDTH;

    //reset underlying pacing interval if we are not continuing underlying pacing
    if (!(shm->continue_underlying)) state.pacing_interval_counter = shm->nominal_pi;

    //state.last_stim_given = rtp_shm->scan_index;
  }

  if ( state.control_pulse_width_counter == 0 ) {
    /* decay and rest */
    comedi_data_write(rtp_comedi_ao_dev_handle, // reset
                      rtp_shm->ao_subdev, 
                      shm->ao_chan, 
                      ao_range, 
                      AREF_GROUND,
                      ao_rest_level);
    state.control_stimulus_called = 0; //turn off control call
  }

  if (state.control_interval_counter >= 0)  state.control_interval_counter--;
  if (state.control_pulse_width_counter >= 0)  state.control_pulse_width_counter--;

}


//this function puts the snapshot on the fifo for the non-real-time application
//to grab and save to disk
static void out_to_fifo(void)
{
  working_snapshot.nominal_pi = shm->nominal_pi;
  //working_snapshot.pi = state.pi;
  working_snapshot.pi = state.previous_apd+state.di;
  working_snapshot.delta_pi = state.delta_pi;
  working_snapshot.ap_ti = state.ap_ti;
  working_snapshot.ap_t90 = state.ap_t90;
  working_snapshot.apd = state.apd;
  working_snapshot.previous_apd = state.previous_apd;
  working_snapshot.di = state.di;
  working_snapshot.previous_di = state.previous_di;
  working_snapshot.vmax = state.vmax_since_last_detected_thresh_crossing;
  //  working_snapshot.vmin = state.vmin_since_last_detected_thresh_crossing;
  working_snapshot.vmin = state.vmin_previous;
  working_snapshot.control_on = shm->control_on;
  working_snapshot.pacing_on = shm->pacing_on;
  working_snapshot.continue_underlying = shm->continue_underlying;
  working_snapshot.target_shorter = shm->target_shorter;
  working_snapshot.consec_alternating = state.consec_alternating;
  working_snapshot.delta_g = shm->delta_g;
  working_snapshot.g_val = shm->g_val;
  working_snapshot.scan_index = rtp_shm->scan_index;

  rtf_put(shm->fifo_minor, &working_snapshot, sizeof(working_snapshot));
}

/****************************************************************************************/
/****************************************************************************************/

/* 
   Be careful modifying most of the stuff below this point 
    A lot of it is RT-Linux related and does not need to be touched. 
    A few things that you might want to touch are noted.
*/

/****************************************************************************************/
/****************************************************************************************/

// leave this alone unless you know what you're doing !!!
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

  return retval;
}

//perhaps you want to change the shared memory initializations
//that are marked by:   //shm initialization you might want to change
static int init_shared_mem(void)
{

  memset(&working_snapshot, 0, sizeof(MCSnapShot));

  shm = 
    (MCShared *) rtos_shm_attach (MC_SHM_NAME,
                                   sizeof(MCShared));
  if (! shm)  return -ENOMEM;

  memset(shm, 0, sizeof(MCShared));

  shm->fifo_minor = -1;
  shm->map_channel = -1;
  shm->g_val = INITIAL_G_VAL;    //shm initialization you might want to change
  shm->delta_g = INITIAL_DELTA_G;   //shm initialization you might want to change
  shm->nominal_pi = INITIAL_PI;   //shm initialization you might want to change
  shm->pacing_on=0;   //shm initialization you might want to change
  shm->control_on=0;   //shm initialization you might want to change
  shm->continue_underlying=0;   //shm initialization you might want to change
  shm->target_shorter=0;   //shm initialization you might want to change

  if ( (shm->ao_chan = 
        find_free_chan(rtp_shm->ao_chans_in_use, rtp_shm->n_ao_chans)) < 0) 
     return -EBUSY; /* no ao chans found */

  shm->magic = MC_SHM_MAGIC;

  return 0;
}

// leave this alone unless you know what you're doing !!!
static int krange_max_is_close(const comedi_krange *k, double voltage)
{
  static const double krange_multiple = 1e-6, tolerance = 0.01;
  double num = ((double)(k->max)) * krange_multiple, diff;
  
  diff = num - voltage;

  if (diff < 0) diff = voltage - num;

  if (diff <= tolerance) return 1;
                           
  return 0;
}

// leave this alone unless you know what you're doing !!!
/* probes shm->ao_chan to find the maximal possible output voltage and
   saves range setting in the static global ao_range */
static int init_ao_chan(void) 
{
  char gotit = 0;
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
 
  for (i = 0, ao_range = 0; i < n_ranges && !gotit; i++) {
    comedi_get_krange(rtp_comedi_ao_dev_handle, rtp_shm->ao_subdev, 
                      shm->ao_chan, i, &krange);
    if (krange.max > max_v || 
        (krange_max_is_close(&krange, STIM_VOLTAGE) && (gotit = 1)) ) {
      ao_range = i;
      ao_stim_level = comedi_get_maxdata(rtp_comedi_ao_dev_handle, 
                                         rtp_shm->ao_subdev, 
                                         shm->ao_chan);
      ao_rest_level = ( krange.min == 0 ? 0 : ao_stim_level / 2 );
      max_v = krange.max;            
    }
  }
  last_ao_chan_used = shm->ao_chan;

  return 0;
}

// leave this alone unless you know what you're doing !!!
void map_control_cleanup (void)
{
  rtp_deactivate_function(do_map_control_stuff);
  rtp_unregister_function(do_map_control_stuff);
  if (shm && shm->fifo_minor >= 0)  rtf_destroy(shm->fifo_minor);
  if (shm && shm->ao_chan >= 0)
    /* indicate that now this channel is free */
    _set_bit(shm->ao_chan, rtp_shm->ao_chans_in_use, 0);

  if (shm)  { rtos_shm_detach(shm); shm = 0; }
}

// leave this alone unless you know what you're doing !!!
static int find_free_chan(volatile char *chans, unsigned int n_chans)
{
  unsigned int i;

  for (i = 0; i < n_chans; i++) 
    if (!_test_bit(i, chans)) return (int)i;
  return -1; /* nothing found */
}

// leave this alone unless you know what you're doing !!!
// except _possibly_ for the last part "safety/constraint checks"
static void setup_analog_output(void)
{
  if (last_ao_chan_used != shm->ao_chan) 
    init_ao_chan();
 
  //  some  safety/constraint checks ... such as making sure the user didn't make delta_g 
  //  an unacceptable value
  if (shm->delta_g < MC_DELTA_G_MIN) shm->delta_g = MC_DELTA_G_MIN;
  else if (shm->delta_g > MC_DELTA_G_MAX) shm->delta_g = MC_DELTA_G_MAX;  
}
