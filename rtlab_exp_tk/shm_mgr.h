/*
 * This file is part of the RT-Linux Multichannel Data Acquisition System
 *
 * Copyright (C) 1999,2000 David Christini
 * Copyright (c) 2001 David Christini, Lyuba Golub, Calin Culianu
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

#ifndef _SHM_MGR_H
#  define _SHM_MGR_H

#include "common.h"
#include "comedi_device.h"
#include "shared_stuff.h"

namespace ShmMgr 
{
  /* TODO: Implement more functionality? Perhaps see about using
           some abstractions to represent comedi channels/gain/range?*/

  extern SharedMemStruct *shm;

  void check(); /* attaches to shm, or throws exception if unable to */
  void detach(); 
  bool attach(); /* returns true iff we sucessfully attached to the shm 
		      or were already attached */
  
  /* channel-specific stuff--basically wrappers to CR_PACK */

  /* below returns range index from comedi - use with comedi_get_range */
  unsigned int channelRange(ComediSubDevice::SubdevType t, 
			    unsigned int chan);   
  unsigned int channelRange(int subdevtype, unsigned int chan);
  void setChannelRange(ComediSubDevice::SubdevType t, unsigned int chan, 
		       unsigned int r); 
  void setChannelRange(int subdevtype, unsigned int chan, unsigned int r);
 

  /* is channel controls */
  bool isChanOn(ComediSubDevice::SubdevType t, unsigned int chan);
  bool isChanOn(int t, unsigned int chan); 
  void setChannel (ComediSubDevice::SubdevType t, unsigned int chan, 
		   bool onoroff);
  void setChannel (int t, unsigned int chan, bool onoroff);
  
  /* return the right array for this type */
  unsigned int *chanArray(ComediSubDevice::SubdevType t);
  unsigned int *chanArray(int subdevtype);
  char *chanUseArray(ComediSubDevice::SubdevType t);
  char *chanUseArray(int t);
};


#endif
