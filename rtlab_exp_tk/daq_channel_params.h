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
#ifndef _DAQ_CHANNEL_PARAM
#define _DAQ_CHANNEL_PARAM

#include <qstring.h>
#include <vector>

#include "spike_polarity.h"

class ECGGraphContainer;
class ComediSubDevice;

/* This struct 'models' a particular channel.  Useful for communicating
   different things about an open channel to different parts of DAQSystem */
struct DAQChannelParams {

    QString label;
    uint id;
    typedef vector<QString> RangeSettings;
    RangeSettings rangeSettings;
    uint rangeSetting;    
    uint secondsVisible;
    uint spikeBlanking;
    SpikePolarity spikePolarity;
    bool paused;
    double spikeThold;
    bool spikeOn;
    bool axesOn;
    bool statusBarOn;
  
  
    DAQChannelParams(const ComediSubDevice *dev = 0);

    void buildRangeStrings(const ComediSubDevice *dev);
    void setNull(bool n) { isnull = n; };
    bool isNull() const { return isnull; };

    static const DAQChannelParams null;
  
   private:
    bool isnull; // means this channel params struct is ignoreable

};












#endif
