/*
 * This file is part of the RT-Linux Multichannel Data Acquisition System
 *
 * Copyright (C) 2002 David Christini, Calin Culianu
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

#include "daq_channel_params.h"
#include "comedi_device.h"

/* the null channel parameter */
const DAQChannelParams DAQChannelParams::null; 


DAQChannelParams::DAQChannelParams(const ComediSubDevice *dev)
  : id(0), rangeSetting(INITIAL_CHANNEL_GAIN), 
    secondsVisible(INITIAL_SECONDS_VISIBLE), 
    spikeBlanking(INITIAL_SPIKE_BLANKING), 
    spikePolarity(Positive), paused(false),
    spikeThold(0.0), spikeOn(false), axesOn(true), statusBarOn(true),
    isnull(true)
{
  if (dev) {
    buildRangeStrings(dev);  
    setNull(false);
  }
}

void DAQChannelParams::buildRangeStrings(const ComediSubDevice *dev)
{
    rangeSettings.resize(dev->ranges().size());
    for (uint i = 0; i < dev->ranges().size(); i++) 
      rangeSettings[i] = ComediRange::buildString(dev->ranges()[i].min, 
                                                  dev->ranges()[i].max, 
                                                  dev->ranges()[i].unit);
    
    
}



