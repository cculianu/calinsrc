/*
 * This file is part of the RT-Linux Multichannel Data Acquisition System
 *
 * Copyright (C) 1999,2000 David Christini
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

#ifndef _RT_PROCESS_H 
#define _RT_PROCESS_H
#define _COMEDILIB_DEPRECATED 

#include "hardware_specific.h" //system specific info (e.g. DAQ board, RAM)
#include <mbuff.h> 

#ifndef _LINUX_COMEDILIB_H
#include <comedilib.h>         //comedi header file
#endif

//#define NUM_AD_CHANNELS_TO_USE NUM_AD_CHANNELS    //# analog input channels 
#define NUM_AD_CHANNELS_TO_USE 8  //# analog input channels 
                                                  //to scan from DAQ board
#define RT_PROCESS_MODULE_NAME "rt_process"
/* Used in SharedMemStruct and SignalParams                                  */
typedef unsigned long int scan_index_t; 

/* 
   The FIFO_struct structure
   -------------------------
   This structure is the output from the DAQ board.  It it put into the 
   /dev/rtf? special device file FIFO's by rt_proccess.c.

   It containts two elements: 

   data -     an array of sample_t which contains the samples obtained from 
              all the channels defined in hardware_specific.h
   checksum - a checksum for data.  Currently this is unused
*/
typedef struct {  
#ifndef FANCYPANTS_READ_METHOD
  lsampl_t data[NUM_AD_CHANNELS_TO_USE];   /* samples for all channels        */
#else
  sampl_t data[NUM_AD_CHANNELS_TO_USE];   /* samples for all channels        */
#endif
  unsigned long checksum;                 /* for checking integrity data     */
} FIFO_struct;

#define RT_HZ 1000             //rt_process loop frequency
#define RT_QUEUE_SIZE (1000*sizeof(FIFO_struct))//size of RT queue
#define RT_PROCESS_SHM_NAME "RT-Linux DAQ system"  //text ID for this process
#define INITIAL_CHANNEL_GAIN 3  //initial channel gain (COMEDI)
#define INITIAL_V_CHANNEL 0     //initial channel for V signal
#define INITIAL_A_CHANNEL 1     //initial channel for A signal

/*
  This structure is used to communicte to the rt process for 
  setting channel parameters on the DAQ board.  Each channel gets one 
  of these  structures.
*/
typedef struct {
  int channel_gain;
  scan_index_t spike_blanking;      
                           /* since_last_spike must be > spike_blanking      */
  int spike_threshold;     /* spike detect. threshold                        */
  scan_index_t num_over_threshold;  
                           /* # of consecutive scans over threshold          */
  scan_index_t since_prev_spike; 
                           /* # of scans since previous spike detected       */
  short spike_polarity;    /* polarity of spike                              */
  scan_index_t spike_index;    
                           /* index of spike                                 */
  scan_index_t found_spike;    
                           /* flag indicating spike has been found 
			      (and when found)                               */
  scan_index_t previous_spike;   
                           /* when previous spike was found                  */
} SignalParams;

/*
  This structure is what gets put into the shared memory segment
  for communication from the non-real time process back to the
  real-time kernel task.
*/
typedef struct {
  SignalParams signal_params [NUM_AD_CHANNELS_TO_USE]; 
                           /* signal parameters, one struct per channel      */

  scan_index_t scan_index; /* index holding current analog input scan number */
  int num_ai_chan_in_use;  /* number of ai channels in use                   */
  // TO DO: get rid of structure member, and find a more
  //        elegant way to handle this! (This should be transparent
  //        to the non-realtime task)
  unsigned int ai_chan[NUM_AD_CHANNELS_TO_USE]; 
                           /* comedi channel structure for Analog Input      */
} SharedMemStruct;


const int NumOverThreshold=1;                //pts above thresh for a spike
const float ScanToMsConversion=1000.0/(float)(RT_HZ); //
const float MsToScanConversion=(float)(RT_HZ)/1000.0; //
const int MaxSpikeBlanking=RT_HZ;    //max interval for disabling 
                                     //v_spike detect = 1 sec
const int MinSpikeBlanking=RT_HZ/50; //minimum interval for disabling
                                     //v_spike detect = (1/50)sec
const int InitSpikeBlanking=RT_HZ/20;//initial interval for disabling
                                     //v_spike detect ... i.e. 1/20 sec, 50ms


//***** note: these defines are for the RT put and get FIFOs
//*****       I only use one put FIFO in the current version, so 
//*****       this is a lot of unnecessary code. 
//*****       But, I've left it for now in case I want to activate more
//*****       FIFOs in the future.

#define LAST_ACTIVE_AD_CHANNEL (NUM_AD_CHANNELS_TO_USE-1) //last AI channel used 
                                                  //for this process
#define NUM_DA_CHANNELS 2        //number of analog output channels 



/*---------------------------------------------------------------------------
  FIFO-RELATED DEFINES 
---------------------------------------------------------------------------*/

#define MAX_RT_FIFO_NO ((unsigned char)64) /* this is from rtl_fifo.h */


/* 
   The following: GET_FIFOS, NUM_GET_FIFOS, PUT_FIFOS, and NUM_PUT_FIFOS
   are all pretty useless defines, but they are in place in case we 
   decide to use more fifos at a later date 

   Note that FIFO identifiers cannot be numbers above MAX_RT_FIFO_NO
*/

/* Defines /dev/rtf0 as the primary fifo for communicating with the
   Non-RT process */
#define __PRIMARY_GET_FIFO ((unsigned int)0)
/* Set of fifos for 'gets' (from the user-process point-of-view), 
   currently only 1 (/dev/rtf0) */
#define GET_FIFOS {__PRIMARY_GET_FIFO}
#define NUM_GET_FIFOS ((unsigned char)1)

/* Set of fifos for puts (currently none) */
#define PUT_FIFOS {}
#define NUM_PUT_FIFOS ((unsigned char)0)

#endif //_RT_PROCESS_H
