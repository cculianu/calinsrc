/*
 * This file is part of the RT-Linux Multichannel Data Acquisition System
 * This contains definitions specific to the NI AT- and PCI- MIO16E DAQ cards 
 * and my computer hardware (e.g., the amount of RAM)
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

/**
   HARDWARE SPECIFIC DEFINES

   Edit this file to suit the specifics of your DAQ board.  Currently
   this file is already set up for a NI_MIO1610E board.
   
   To Do: Make the source or the compiled app auto-configure itself
          based on the board you decide to use, either by using
	  existing comedi drivers, or through other means. That
	  would eliminate the need for this non-user-friendly
	  way of telling the sourcecode about how to use
	  your board
*/
          

#ifndef _NI_MIO16E10_H 
#define _NI_MIO16E10_H 

#define SCAN_UNITS 4096	       //NI_MIO1610E scanning resolution (12bit) 
#define NUM_AD_CHANNELS 16      //number of analog input channels
#define NUM_GAIN_PARAMETERS 15 // the number of gain settings this device has

#define AI_DEV 0      // *** set per how you comedi_config'd your board
#define AI_SUBDEV 0   // *** set per results of comedilib/demo/info 
#define AO_DEV 0      // *** set per how you comedi_config'd your board
#define AO_SUBDEV 1   // *** set per results of comedilib/demo/info 


typedef struct {
  float gainSettings[2][NUM_GAIN_PARAMETERS+1];    /* [0][x] -> negative, 
						      [1][x] -> positive 
						      for that index */
  const char *gainNames[NUM_GAIN_PARAMETERS+1];    /* The descriptive
						      text names of each
						      gain setting */
} HardwareSpecificStruct;

/* Set the three following defines for your board, then use
   the HARDWARE_SPECIFIC_INIT define to initialize a 
   HardwareSpecific struct. */

/*--------------------------------------------------------------------------
  HardwareSpecificStruct initializers.
----------------------------------------------------------------------------*/

/*  The following two are gain related: */

/* the low, mid, and high voltages corresponding to the NI_MIO1610E's 
   gain parameters, 0,1,2, ... 14 */
#define _DAQ_DEVICE_GAIN_SETTINGS \
{ {-10,-5,-2.5,-1,-0.5,-0.25,-0.1,-0.05,0,0,0,0,0,0,0}, \
  {+10,+5,+2.5,+1,+0.5,+0.25,+0.1,+0.05,+10,+5,+2,+1,+0.5,+0.20,+0.1} }

/* This array's indices should correspond to the above gain array.
   The strings from here end up in the text boxes in the daq_system
   application */
#define _DAQ_DEVICE_GAIN_NAMES { \
    "-10V,+10V", "-5V,+5V", "-2.5V,+2.5V", \
    "-1V,+1V", "-500mV,+500mV", "-250mV,+250mV", \
    "-100mV,+100mV", "-50mV,+50mV", "0V,+10V", \
    "0V,+5V", "0V,+2V", "0V,+1V", "0V,+500mV", \
    "0V,+200mV", "0V,+100mV", 0 }

/*--------------------------------------------------------------------------
  This compiler sybmol is used to initialize 
  HardwareSpecificStruct instances. It is important
  to note that if you change any of the data for this, you need
  to recompile all the .o's in this application. 
----------------------------------------------------------------------------*/
#define DAQ_HARDWARE_SPECIFIC_STRUCT_INIT { _DAQ_DEVICE_GAIN_SETTINGS, \
                                            _DAQ_DEVICE_GAIN_NAMES }


#endif
