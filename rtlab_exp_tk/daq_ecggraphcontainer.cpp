/*
 * This file is part of the RT-Linux Multichannel Data Acquisition System
 *
 * Copyright (C) 1999,2000 David Christini
 * Copyright (C) 2001 David Christini, Lyuba Golub, Calin Culianu
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
#include "ecggraph.h"
#include "daq_ecggraphcontainer.h"

DAQECGGraphContainer::
DAQECGGraphContainer(ECGGraph *graph, 	
		     unsigned int chan,
		     QWidget *parent = 0, 
		     const char *name = 0, 
		     WFlags flags = 0)
  : ECGGraphContainer(graph, parent, name, flags | WDestructiveClose), 
    channel_id(chan)

{
  this->setCaption(graph->name());
  /* delete the defualt range setting as it breaks daq_system */
  deleteRangeSetting(0);
  connect(this, SIGNAL(rangeChanged(int)), 
	  this, SLOT (_rangeChangedWrapper(int)));
}


void
DAQECGGraphContainer::
closeEvent (QCloseEvent *e) 
{
  emit closing(channelId()); /* this tells daqsystem to remove us from 
				the channel list */
  ECGGraphContainer::closeEvent(e);
}  

void
DAQECGGraphContainer::
_rangeChangedWrapper(int range)
{
  emit rangeChanged(channelId(), range);
}
