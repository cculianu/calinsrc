/*
 * This file is part of the RT-Linux Multichannel Data Acquisition System
 *
 * Copyright (c) 2001 Calin Culianu
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


#ifndef _PROBE_H
#define _PROBE_H

#include <qstring.h>
#include <vector>
#include "common.h"
#include "exception.h"
#include "comedi_device.h"

/* PROBE
   A pretty basic class to probe our environment. 
   Figures out what boards we have, whether rt_process is loaded, etc. 

   TODO: Add more robust board capability probing into this class
   so that our future SampleStructComediSource class will be able
   to intelligently use comedi commands based on the probed information
   about the board given to it by this Probe class. */
class Probe 
{
 public:

  /* Warning: this constructor may fail and throw a NoComediDeviceException
     if probe finds no candidate comedi devices! */
  Probe(bool allowBusyDevices = false, 
	const QString & prefix = "/dev/comedi"); 

  bool have_rt_process;
  vector<ComediDevice> probed_devices;

  /* Find a comedi device by device file name, returns the null device if not
     found */
  const ComediDevice & find(const QString & dev) const;

 private:  
  void do_probe(const QString & filename); /* throws NoComediDeviceException
					      if filename is not a comedi dev, 
					      or no driver for filename */
  void validate() const; /* throws NoComediDeviceException
			    if this object is in a state that indicates comedi 
			    devices are  busy/unusable/nonexistant */


  void attach_to_shm_and_stuff(); /* called from init. */
  void kill_shm(); /* tries to detach shm if attached */
  bool allowBusyDevices;
};



#endif
