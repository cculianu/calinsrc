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

#include "comedi_device.h"

/* below returns a ComediSubDevice with isNull()==true if not found */
const ComediSubDevice &
ComediDevice::find(ComediSubDevice::SubdevType type, int id_start = 0) const
{
  static const ComediSubDevice nullSubDevice((float *)0);
  for (unsigned int i = 0; i < subdevices.size(); i++) {
    if (subdevices[i].id >= id_start && subdevices[i].type == type) {
      return subdevices[i];
    }
  }
  /* if it's not found */
  return nullSubDevice;
}

/* modifies SampleStruct, writing data into it as well */
lsampl_t
ComediSubDevice::
unitsToSample(SampleStruct & s, double unitdata) const
{
  return (s.data = (lsampl_t)
	  ((unitdata-(_ranges[s.range].min) 
	    / (_ranges[s.range].max - _ranges[s.range].min)) 
	   * maxdata));
}
