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
#include "daq_ecggraphcontainer.h"


/* ugly non-class-specific static constants but this saves needing to duplicate this work in the .h file */
static const QString 
  MOUSE_POS_FORMAT("Mouse pos: %1 V at scan %2"),
  CURRENT_INDEX_FORMAT("Scan Index: %1"),
  SPIKE_THOLD_FORMAT("Spike Threshold: %1 V"),
  LAST_SPIKE_FORMAT("Last Spike %1 V at %2");
  

DAQECGGraphContainer::
DAQECGGraphContainer(ECGGraph *graph, 	
		     uint chan,
		     QWidget *parent = 0, 
		     const char *name = 0, 
		     WFlags flags = 0,
		     uint64 scanIndexStatusIncrement = 1000)
  : ECGGraphContainer(graph, parent, name, flags | WDestructiveClose),
    channelId(chan), 
    scan_index_threshhold(scanIndexStatusIncrement), 
    last_scan_index((uint64)-scan_index_threshhold)

{
  if (name)  this->setCaption(name);
  /* delete the defualt range setting as it breaks daq_system */
  deleteRangeSetting(0);
  connect(this, SIGNAL(rangeChanged(int)), 
	  this, SLOT (_rangeChangedWrapper(int)));

  // for the status bar..
  statusBar = new QStatusBar( this );
  statusBar->setSizeGripEnabled( false );

  /* this _tries_ to add the status bar to the bottom... 
     not elegant design but what he hey?? */
  layout->addMultiCellWidget( statusBar, 
			      layout->numRows(), layout->numRows(),
			      0, layout->numCols()-1 );

  {
    /* populate that status bar with them gosh-darned labels and signal/slot
       connections */

    currentIndex = new QLabel(CURRENT_INDEX_FORMAT.arg("-"), statusBar);
    mouseOverVector = new QLabel(MOUSE_POS_FORMAT.arg("-").arg("-"),statusBar);
    spikeThreshHold = new QLabel(statusBar);       
    lastSpike = new QLabel(LAST_SPIKE_FORMAT.arg("-").arg("-"), statusBar);

    if ( graph->spikeMode() ) {
      setSpikeThreshHoldStatus(graph->spikeThreshHold());
    } else {
      unsetSpikeThreshHoldStatus();
    }
    
    statusBar->addWidget(currentIndex);
    statusBar->addWidget(mouseOverVector);
    statusBar->addWidget(spikeThreshHold);
    statusBar->addWidget(lastSpike);

    connect(graph, SIGNAL(mouseOverVector(double, uint64)),
	    this, SLOT(setMouseVectorStatus(double, uint64)));
    connect(graph, SIGNAL(spikeThreshHoldSet(double)),
	    this, SLOT(setSpikeThreshHoldStatus(double)));      
    connect(graph, SIGNAL(spikeThreshHoldUnset()),
	    this, SLOT(unsetSpikeThreshHoldStatus()));
    connect(graph, SIGNAL(spikeDetected(uint64, double)),
	    this, SLOT(setLastSpikeStatus(uint64, double)));
  }
}


void
DAQECGGraphContainer::
closeEvent (QCloseEvent *e) 
{
  emit closing(this); /* This tells daqsystem to remove us from 
			 the channel list */
  ECGGraphContainer::closeEvent(e);
}  

void
DAQECGGraphContainer::
_rangeChangedWrapper(int range)
{
  emit rangeChanged(channelId, range);
}

void
DAQECGGraphContainer::
consume(const SampleStruct *sample)
{
  graph->plot(sample->data, sample->scan_index);
  setCurrentIndexStatus(sample->scan_index);
}


/* Slot for updating the 'Mouse pos' status bar line.
   The 'index' we get here (from the ECGGraph class) is inaccurate
   as dropped samples can lead to de-synchronization between
   the graph and the actual sample's scan index.
   In addition we may also have O.B.1 (Obi-Wan) error here -- I didn't 
   check since I will re-write this mechanism soon.  

   TODO: Rethink scan index strategy  --Calin */
void
DAQECGGraphContainer::
setMouseVectorStatus(double voltage, uint64 index)
{   
  mouseOverVector->setText(MOUSE_POS_FORMAT.arg(voltage).arg((long int)index));
}

void
DAQECGGraphContainer::
setCurrentIndexStatus(uint64 index)
{
  if (index - last_scan_index >= scan_index_threshhold) {
    last_scan_index = index;
    currentIndex->setText(CURRENT_INDEX_FORMAT.arg((ulong) index));
  }
}

void
DAQECGGraphContainer::
setSpikeThreshHoldStatus(double voltage)
{
  spikeThreshHold->setText(SPIKE_THOLD_FORMAT.arg(voltage));
}

void
DAQECGGraphContainer::
unsetSpikeThreshHoldStatus()
{
  spikeThreshHold->setText(SPIKE_THOLD_FORMAT.arg("-"));
}

void
DAQECGGraphContainer::
setLastSpikeStatus(uint64 index, double voltage)
{
  lastSpike->setText(LAST_SPIKE_FORMAT.arg(voltage).arg((ulong) index));
}
