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
          

#ifndef _HARDWARE_SPECIFIC_H 
#define _HARDWARE_SPECIFIC_H 

#define __DAS_1602 "das1602"
#define __NIMIO "nimio"

#ifdef HW
#  if  HW == __DAQ_1602
#    include "hardware_specific_das1602.h"
#  else
#    include "hardware_specific_nimio.h"
#  endif
#else
#  include "hardware_specific_nimio.h"
#endif

#endif
