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
#include "apd_control.h"

#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL"); 
#endif

#define MODULE_NAME "MC Stim"

MODULE_AUTHOR("David J. Christini, PhD and Calin A. Culianu [not PhD :(]");
MODULE_DESCRIPTION(MODULE_NAME ": A Real-Time stimulation and control add-on for daq system and rtlab.o.\n$Id$");

int apd_control_init(void); 
void apd_control_cleanup(void);

static int init_shared_mem(void);
static int init_ao_chan(void);

static void do_apd_control_stuff(MultiSampleStruct *);

/* 'inner' helper functions... */
static void setup_analog_output(void);
static void do_pacing(int);  
static void calculate_apd_and_control_perturbation(int,MultiSampleStruct *);
static void do_control(int); 
static void automatically_adapt_g(int);
static void out_to_fifo(int,int);
static int find_free_chan(volatile char *, unsigned int);

module_init(apd_control_init);
module_exit(apd_control_cleanup);

static MCShared *shm = 0;  //pointer to the shared memory ... communicates between real-time
                                              // process and non-real-time GUI
static MCSnapShot  working_snapshot; /* used for inter-function comm.  btwn RT and non-RT GUI  */

static int          last_ao_chan_used = -1;
static unsigned int ao_range = 0;
static lsampl_t     ao_stim_level = 0, /* basically comedi_get_maxdata()- 1 */
                    ao_rest_level = 0; /* either ao_stim_level / 2 or 0 */

/*-------------------------------------------------------------------- 
  Some initial parameters or otherwise private constants...                 
  Tweak these to suit your taste...
---------------------------------------------------------------------*/
static const  int STIM_PULSE_WIDTH = 2;      /* in milliseconds              */
static const float STIM_VOLTAGE       = 5.0;   /* Preferred stim voltage       */
static const float INITIAL_DELTA_G    = 0.01; /* for MCShared init.           */
static const float INITIAL_G_VAL          = 0.5;   /* "   "         "           */
static const int MS_AFTER_THRESH_TO_LOOK_FOR_PEAK = 25; // time (ms) to look for AP peak after threshold crossing
static const int INIT_APD_XX=90;
static const int INITIAL_PI=500;
#define RESET_V_BASELINE 999.0  //set v_baseline to very large value, must use define for StimState initializer
#define RESET_V_APA -999.0      //set v_apa to very small value, must use define for StimState initializer
/*-------------------------------------------------------------------*/

/* NB: This module needs at least a 1000 hz sampling rate!
   It will fail if that is not the case at module initialization, 
   and may produce undefined results if that is not the case while the 
   module is running. */
// ***** this should be changes so sampling rate is not hard wired
static const int REQUIRED_SAMPLING_RATE = 1000;

//structure storing the state of the system so that one scan knows the
//state from the previous scan
struct APDState {    //look in apd_control.h and apd_control.cpp for comments on these variables
  scan_index_t scan_index;
  int find_peak;
  double v_baseline_since_last_detected_thresh_crossing;
  double v_baseline_n_minus_1;   //v_baseline for the previous AP
  double v_baseline_n_minus_2;   //v_baseline for the (n-2) AP
  double v_apa;                              //action potential amplitude
  double v_xx;
  scan_index_t ap_ti;
  scan_index_t ap_tf;
  int apd;
  int previous_apd;
  int di;
  int previous_di;
};

struct StimState {    //look in apd_control.h and apd_control.cpp for comments on these variables
  scan_index_t scan_index;
  int pacing_pulse_width_counter;
  int pacing_interval_counter;
  int control_stimulus_called;
  int control_interval_counter;
  int control_pulse_width_counter;
  int pi;
  int delta_pi;
  int consec_alternating;
  int previous_perturbation_signs[4];
};

struct APDState apd_state[NumAPDs],
                            default_apd_state = {scan_index:0, 
			  find_peak:0, 
			  v_baseline_since_last_detected_thresh_crossing:RESET_V_BASELINE,
			  v_baseline_n_minus_1:RESET_V_BASELINE,
			  v_baseline_n_minus_2:RESET_V_BASELINE,
			  v_apa:RESET_V_APA,
			  v_xx:0, 
			  ap_ti:0, 
			  ap_tf:0, 
			  apd:0, 
			  previous_apd:0, 
			  di:0, 
			  previous_di:0,
};

struct StimState stim_state[NumAOchannels],
                            default_stim_state = {scan_index:0, 
			  pacing_pulse_width_counter:0, 
			  pacing_interval_counter:0, 
			  control_stimulus_called:0, 
			  control_interval_counter:0, 
			  control_pulse_width_counter:-1,
			  pi:0, 
			  delta_pi:0, 
			  consec_alternating:0,
			  previous_perturbation_signs:{1,0,1,0} /*init to alternating*/
};

/****************************************************************************************/
/* do_apd_control_stuff is the main "brain" of the pacing and control system. 
   this function gets run once every analog acquisition scan ...
   so it, and its sub-functions, are  where most algorithmic changes will need 
    to be made. */
/****************************************************************************************/
static void do_apd_control_stuff (MultiSampleStruct * m)
{

  int which_ai_chan;

  /* Note that do_pacing assumes that rt_process.o is running at precisely 1000 hz! */
  setup_analog_output();

  do_pacing(0); //pacing for AOchan 0
  do_pacing(1); //pacing for AOchan 1

  for (which_ai_chan=0; which_ai_chan<NumAPDs; which_ai_chan++)
    if (is_chan_on(which_ai_chan,rtp_shm->ai_chans_in_use))
	calculate_apd_and_control_perturbation(which_ai_chan,m);

  if (stim_state[0].control_stimulus_called) do_control(0);
  if (stim_state[1].control_stimulus_called) do_control(1);

  m = 0; /* to avoid compiler errors ... not sure what this does ... Calin put it here */
}

// this function does periodic pacing at the PI controlled by the user in the GUI
// via the shm->nominal_pi shared memory variable
static void do_pacing(int which_ao_chan)
{ 

  if ( (stim_state[which_ao_chan].pacing_interval_counter == 0) && 
       (shm->pacing_on[which_ao_chan]) ) { 
    /* polarization (begin stimulation) */
    comedi_data_write(rtp_comedi_ao_dev_handle, 
                      rtp_shm->ao_subdev, 
                      which_ao_chan,
                      ao_range, 
                      AREF_GROUND,
                      ao_stim_level);
    stim_state[which_ao_chan].pacing_interval_counter = shm->nominal_pi[which_ao_chan];
    stim_state[which_ao_chan].pacing_pulse_width_counter = STIM_PULSE_WIDTH;
  }

  if ( stim_state[which_ao_chan].pacing_pulse_width_counter == 0 ) {
    /* decay and rest */
    comedi_data_write(rtp_comedi_ao_dev_handle, // reset
                      rtp_shm->ao_subdev, 
                      which_ao_chan,
                      ao_range, 
                      AREF_GROUND,
                      ao_rest_level);    
  }

  if (stim_state[which_ao_chan].pacing_interval_counter > 0)  stim_state[which_ao_chan].pacing_interval_counter--;
  if (stim_state[which_ao_chan].pacing_pulse_width_counter > 0)  stim_state[which_ao_chan].pacing_pulse_width_counter--;

}

//self-explanatory function title
static void calculate_apd_and_control_perturbation(int which_ai_chan, MultiSampleStruct * m)
{

  int which_ao_chan;
  int yes_do_control;

  //voltage_at used MultiSampleStruct * m to return the voltage of the requested channel.
  double this_voltage=voltage_at(which_ai_chan,m);

  //look for v_baseline on every scan ... this may be overkill, but it's safest in case signal drifts
  //and we miss an AP (i.e., if we only looked during part of the AP cycle and we miss an AP
  //then we may stop looking for v_baseline)
  if (this_voltage < apd_state[which_ai_chan].v_baseline_since_last_detected_thresh_crossing)
    apd_state[which_ai_chan].v_baseline_since_last_detected_thresh_crossing = this_voltage;

  // is there a threshold crossing occuring at this scan? ... the conditional is Calin's code
  if ( ( _test_bit((int)which_ai_chan, spike_info.spikes_this_scan)) ) {
    apd_state[which_ai_chan].find_peak=1;                           //if so, set find_peak so that we know to start looking for peak amplitude
    apd_state[which_ai_chan].ap_ti=rtp_shm->scan_index; //this scan is the beginning of the action potential, so set ap_ti    

    apd_state[which_ai_chan].v_baseline_n_minus_2 = apd_state[which_ai_chan].v_baseline_n_minus_1; //store previous as n_minus_2
    apd_state[which_ai_chan].v_baseline_n_minus_1 = apd_state[which_ai_chan].v_baseline_since_last_detected_thresh_crossing; //store
    apd_state[which_ai_chan].v_baseline_since_last_detected_thresh_crossing = RESET_V_BASELINE; //reset
    apd_state[which_ai_chan].v_apa=RESET_V_APA; //reset
  }

  //check this scan to see if it is the peak AP voltage ... this search will be run for each scan
  //after threshold crossing occurred until MS_AFTER_THRESH_TO_LOOK_FOR_PEAK ms 
  //after the threshold  crossing occurred
  while (apd_state[which_ai_chan].find_peak && apd_state[which_ai_chan].find_peak<MS_AFTER_THRESH_TO_LOOK_FOR_PEAK) { 
    if (this_voltage > apd_state[which_ai_chan].v_apa)
      apd_state[which_ai_chan].v_apa = this_voltage; 
    apd_state[which_ai_chan].find_peak++;
    //now that we've found peak voltage, compute the voltage for apd90
    if (apd_state[which_ai_chan].find_peak==MS_AFTER_THRESH_TO_LOOK_FOR_PEAK) { 
      double which_baseline_to_use=apd_state[which_ai_chan].v_baseline_n_minus_1;
      // ******* NOTE: should the next conditional be using < instead of > ?
      // ******* currently baseline is set at the larger of the 2 previous baselines.
      if (apd_state[which_ai_chan].v_baseline_n_minus_2 > which_baseline_to_use) 
	which_baseline_to_use = apd_state[which_ai_chan].v_baseline_n_minus_2;
      apd_state[which_ai_chan].v_xx = (shm->apd_xx)*(apd_state[which_ai_chan].v_apa - which_baseline_to_use) + which_baseline_to_use;

    }
  }

  // is this scan the first time we cross back below apd90 ... if so, determine apd and do other
  // stuff, including compute control perturbation
  // obviously, this process can only occur after the peak voltage was found in the previous section
  if ((apd_state[which_ai_chan].find_peak==MS_AFTER_THRESH_TO_LOOK_FOR_PEAK) && (this_voltage<apd_state[which_ai_chan].v_xx)) {
    apd_state[which_ai_chan].find_peak=0;
    //set previous_apd and previous_di before we make changes to new di and new apd
    apd_state[which_ai_chan].previous_apd = apd_state[which_ai_chan].apd;
    apd_state[which_ai_chan].previous_di=apd_state[which_ai_chan].di;
    //set new di before we change ap_tf (because we need to use previous ap_tf)
    apd_state[which_ai_chan].di=apd_state[which_ai_chan].ap_ti-apd_state[which_ai_chan].ap_tf; //this DI is time since last AP_tf;
    //for new apd
    apd_state[which_ai_chan].ap_tf=rtp_shm->scan_index;
    apd_state[which_ai_chan].apd = apd_state[which_ai_chan].ap_tf-apd_state[which_ai_chan].ap_ti;

    //Determine if this channel is one that is being monitored by one of the 2 AO channels
    which_ao_chan=-1;
    yes_do_control=0;
    //is this the channel that is being monitored for ao0?
    if (which_ai_chan == shm->ai_chan_this_ao_chan_is_dependent_on[0]) { 
      which_ao_chan=0;
      if (shm->control_on[which_ao_chan]) yes_do_control=1;
    }
    //if not, is this the channel that is being monitored for ao1?
    else if (which_ai_chan == shm->ai_chan_this_ao_chan_is_dependent_on[1])  { 
      which_ao_chan=1;
      if (shm->control_on[which_ao_chan]) yes_do_control=1;
    }

    // compute control perturbation if yes_do_control
    if (yes_do_control) { 

      //this is for automatic g adaptation .. shift array of previous perturbation sign values 
      stim_state[which_ao_chan].previous_perturbation_signs[0] = 
	stim_state[which_ao_chan].previous_perturbation_signs[1];
      stim_state[which_ao_chan].previous_perturbation_signs[1] = 
	stim_state[which_ao_chan].previous_perturbation_signs[2];
      stim_state[which_ao_chan].previous_perturbation_signs[2] = 
	stim_state[which_ao_chan].previous_perturbation_signs[3];

      // ****** note minus sign inserted in  front of g_val!!!  djc: 9/25/02
      stim_state[which_ao_chan].delta_pi = -(int)(shm->g_val[which_ao_chan]*(apd_state[which_ai_chan].previous_apd - apd_state[which_ai_chan].apd));

      if (  (!( shm->only_negative_perturbations[which_ao_chan])) ||  //if positive perturbs allowed 
            ( (shm->only_negative_perturbations[which_ao_chan])&&(stim_state[which_ao_chan].delta_pi<=(-1)) ) ) { //or if only negative allowed and this perturb <0
	stim_state[which_ao_chan].control_stimulus_called=1;
	// control stimulus must occur at the time expected for the next pacing stimulus
	// (which is given by stim_state[which_ao_chan].pacing_interval_counter ... which at the current scan should be equal
	//   to nominal_pi - apd), plus the perturbation (which is negative).
	stim_state[which_ao_chan].control_interval_counter = stim_state[which_ao_chan].pacing_interval_counter+stim_state[which_ao_chan].delta_pi;

	//special case for AO1 ... the case where control of AO1 is linked to the stimulation
	//at AO0, i.e., it's a certain time after (or before) AO1 stimulation
	if ( (which_ao_chan==1)&&(shm->link_to_ao0) ) {
	  //delta_pi cannot be larger than the conduction time (or less than -1 times cond. time)
	  //otherwise one of the sites (whichever is stimulated last) will be refractory to its
	  //own stimulus due to the previous arrival of the stimuls from the other site..
	  if ( stim_state[1].delta_pi > shm->ao0_ao1_cond_time )
	    stim_state[1].delta_pi = shm->ao0_ao1_cond_time - 1;
	  if ( stim_state[1].delta_pi < (-1*(shm->ao0_ao1_cond_time)) )
	    stim_state[1].delta_pi = 1 - shm->ao0_ao1_cond_time;

	  //control interval is the pacing interval for AO0 plus the perturbation to AO0 pacing
	  //(if there is  a perturbation, that is), plus this perturbation
	  stim_state[1].control_interval_counter = stim_state[0].pacing_interval_counter+stim_state[0].delta_pi+stim_state[1].delta_pi;

	}

	//make pacing_interval_counter > control_interval_counter if this is a pos. perturbation
	//so that the natural pacing doesn't precede control (unless we're continue_underlying)
	if ( (stim_state[which_ao_chan].delta_pi>=1) && (!(shm->continue_underlying[which_ao_chan])) )
	  stim_state[which_ao_chan].pacing_interval_counter = stim_state[which_ao_chan].control_interval_counter + 1;

	stim_state[which_ao_chan].previous_perturbation_signs[3]=0;
      }
      else  stim_state[which_ao_chan].previous_perturbation_signs[3]=1;

      if ((shm->g_adjustment_mode[which_ao_chan]) == MC_G_ADJ_AUTOMATIC) automatically_adapt_g(which_ao_chan); 

    }
    //write to fifo, which in apd_control.cpp will be saved to disk
    out_to_fifo(which_ai_chan,which_ao_chan);  // write snapshot to fifo at the end of every action potential
                             // note that end of ap was chosen arbitrarily .. could have been beginning
                             // may make more sense to do it whenever a stimulus is delivered

  }  //end of   if ((apd_state.find_peak==MS_AFTER_THRESH_TO_LOOK_FOR_PEAK) && ...

}

// this function performs automatic adaptation of g (control proportionality constant)
// according to the algorithm in Hall&Christini, PRE, vol. 63, article 06204, 2001
static void automatically_adapt_g(int which_ao_chan) {

  //if perturbation sign has not alternated 4 times in a row, then we are overperturbing,
  //meaning our g is too large, so we should decrease g.
  if ( (stim_state[which_ao_chan].previous_perturbation_signs[3] == stim_state[which_ao_chan].previous_perturbation_signs[2]) ||
       (stim_state[which_ao_chan].previous_perturbation_signs[2] == stim_state[which_ao_chan].previous_perturbation_signs[1]) ||
       (stim_state[which_ao_chan].previous_perturbation_signs[1] == stim_state[which_ao_chan].previous_perturbation_signs[0]) ) {
    shm->g_val[which_ao_chan] -= shm->delta_g[which_ao_chan];      // decrease g if perturbation sign did not alternating 4 times in a row
                                                            // i.e., decrease the size of perturbations
    stim_state[which_ao_chan].consec_alternating = 0; 
  }
  else {
    shm->g_val[which_ao_chan] += shm->delta_g[which_ao_chan]; // else perturbation sign has alternated 4 times in  a row,
                                                       // meaning we are underperturbing, so we increase g
    stim_state[which_ao_chan].consec_alternating = 4;
  }

  // don't let g be < 0
  if (shm->g_val[which_ao_chan] < 0.0) shm->g_val[which_ao_chan] = 0;

}

// this function does alternans control
static void do_control(int which_ao_chan) { 

  if ( (stim_state[which_ao_chan].control_interval_counter == 0) ) { 
    /* polarization (begin stimulation) */
    comedi_data_write(rtp_comedi_ao_dev_handle, 
                      rtp_shm->ao_subdev, 
                      which_ao_chan, 
                      ao_range, 
                      AREF_GROUND,
                      ao_stim_level);
    stim_state[which_ao_chan].control_pulse_width_counter = STIM_PULSE_WIDTH;

    //reset underlying pacing interval if we are not continuing underlying pacing
    if (!(shm->continue_underlying[which_ao_chan])) 
      stim_state[which_ao_chan].pacing_interval_counter = shm->nominal_pi[which_ao_chan];
  }

  if ( stim_state[which_ao_chan].control_pulse_width_counter == 0 ) {
    /* decay and rest */
    comedi_data_write(rtp_comedi_ao_dev_handle, // reset
                      rtp_shm->ao_subdev, 
                      which_ao_chan, 
                      ao_range, 
                      AREF_GROUND,
                      ao_rest_level);
    stim_state[which_ao_chan].control_stimulus_called = 0; //turn off control call
  }

  if (stim_state[which_ao_chan].control_interval_counter >= 0)  
    stim_state[which_ao_chan].control_interval_counter--;
  if (stim_state[which_ao_chan].control_pulse_width_counter >= 0)  
    stim_state[which_ao_chan].control_pulse_width_counter--;

}

//this function puts the snapshot on the fifo for the non-real-time application
//to grab and save to disk
static void out_to_fifo(int which_ai_chan, int which_ao_chan) {

  working_snapshot.apd_channel = which_ai_chan;
  working_snapshot.scan_index = rtp_shm->scan_index;

  working_snapshot.apd_xx = (int)(100*(1.0 - shm->apd_xx));
  working_snapshot.v_apa = apd_state[which_ai_chan].v_apa;
  working_snapshot.v_baseline = apd_state[which_ai_chan].v_baseline_n_minus_1;
  working_snapshot.ap_ti = apd_state[which_ai_chan].ap_ti;
  working_snapshot.ap_tf = apd_state[which_ai_chan].ap_tf;  
  working_snapshot.apd = apd_state[which_ai_chan].apd;
  working_snapshot.di = apd_state[which_ai_chan].di;
  working_snapshot.link_to_ao0 = shm->link_to_ao0;
  working_snapshot.ao0_ao1_cond_time = shm->ao0_ao1_cond_time;

  working_snapshot.ao_chan=which_ao_chan;
  if (which_ao_chan >= 0) {
    working_snapshot.nominal_pi = shm->nominal_pi[which_ao_chan];
    working_snapshot.pi = apd_state[which_ai_chan].previous_apd+apd_state[which_ai_chan].di;
    working_snapshot.delta_pi = stim_state[which_ao_chan].delta_pi;
    working_snapshot.control_on = shm->control_on[which_ao_chan];
    working_snapshot.only_negative_perturbations = shm->only_negative_perturbations[which_ao_chan];
    working_snapshot.pacing_on = shm->pacing_on[which_ao_chan];
    working_snapshot.continue_underlying = shm->continue_underlying[which_ao_chan];
    working_snapshot.target_shorter = shm->target_shorter[which_ao_chan];
    working_snapshot.consec_alternating = stim_state[which_ao_chan].consec_alternating;
    working_snapshot.delta_g = shm->delta_g[which_ao_chan];
    working_snapshot.g_val = shm->g_val[which_ao_chan];
  }
  else    {
    working_snapshot.nominal_pi = -1;
    working_snapshot.pi = -1;
    working_snapshot.delta_pi = -1;
    working_snapshot.control_on = -1;
    working_snapshot.only_negative_perturbations = -1;
    working_snapshot.pacing_on = -1;
    working_snapshot.continue_underlying = -1;
    working_snapshot.target_shorter = -1;
    working_snapshot.consec_alternating = -1;
    working_snapshot.delta_g = -1;
    working_snapshot.g_val = -1;
  }

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
int apd_control_init (void)
{
  int retval = 0;
  int i;

  //initialize structs
  for (i=0; i<NumAPDs; i++) memcpy(&apd_state[i],&default_apd_state,sizeof(struct APDState));
  for (i=0; i<NumAOchannels; i++) memcpy(&stim_state[i],&default_stim_state,sizeof(struct StimState));

  if (rtp_shm->sampling_rate_hz < REQUIRED_SAMPLING_RATE) {
    printk("apd_control: cannot start the module because the sampling rate of "
           "rt_process is not %d hz! apd_control.o *requires* a %d hz period "
           "on the rt loop for its own internal simplicity.  The current "
           "rate that rt_process is looping at is: %d", 
           REQUIRED_SAMPLING_RATE, REQUIRED_SAMPLING_RATE, 
           (int)rtp_shm->sampling_rate_hz);
    return -ETIME;
  }

  if ( (retval = rtp_register_function(do_apd_control_stuff))
       || (retval = init_shared_mem())
       || (retval = rtp_find_free_rtf(&shm->fifo_minor, MC_FIFO_SZ))
       || (retval = init_ao_chan()) 
       || (retval = rtp_activate_function(do_apd_control_stuff))
    ) 
    apd_control_cleanup();  

  return retval;
}

//perhaps you want to change the shared memory initializations
//that are marked by:   //shm initialization you might want to change
static int init_shared_mem(void)
{
  int i;

  memset(&working_snapshot, 0, sizeof(MCSnapShot));

  shm = 
    (MCShared *) rtos_shm_attach (MC_SHM_NAME,
                                   sizeof(MCShared));
  if (! shm)  return -ENOMEM;

  memset(shm, 0, sizeof(MCShared));

  shm->fifo_minor = -1;

  for (i=0; i<NumAOchannels; i++) {
    shm->ai_chan_this_ao_chan_is_dependent_on[i] = -1;
    shm->g_val[i] = INITIAL_G_VAL;    //shm initialization you might want to change
    shm->delta_g[i] = INITIAL_DELTA_G;   //shm initialization you might want to change
    shm->nominal_pi[i] = INITIAL_PI;   //shm initialization you might want to change
    shm->pacing_on[i]=0;   //shm initialization you might want to change
    shm->control_on[i]=0;   //shm initialization you might want to change
    shm->continue_underlying[i]=0;   //shm initialization you might want to change
    shm->only_negative_perturbations[i]=1;  //shm initialization you might want to change
    shm->target_shorter[i]=0;   //shm initialization you might want to change
  }
  shm->apd_xx=(1.0 - (0.01*INIT_APD_XX));      //shm initialization you might want to change
  shm->ao0_ao1_cond_time=5;
  shm->link_to_ao0=0;

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
void apd_control_cleanup (void)
{
  rtp_deactivate_function(do_apd_control_stuff);
  rtp_unregister_function(do_apd_control_stuff);
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
  int i;

  if (last_ao_chan_used != shm->ao_chan) 
    init_ao_chan();
 
  //  some  safety/constraint checks ... such as making sure the user didn't make delta_g 
  //  an unacceptable value
  for (i=0; i<NumAOchannels; i++) {
    if (shm->delta_g[i] < MC_DELTA_G_MIN) shm->delta_g[i] = MC_DELTA_G_MIN;
    else if (shm->delta_g[i] > MC_DELTA_G_MAX) shm->delta_g[i] = MC_DELTA_G_MAX;  
  }
}
