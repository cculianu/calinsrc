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

#ifndef _DAQ_ECGGRAPHCONTAINER_H
#define _DAQ_ECGGRAPHCONTAINER_H

#include <qstatusbar.h>

#include "common.h"
#include "ecggraph.h"
#include "ecggraphcontainer.h"
#include "sample_consumer.h"


class DAQECGGraphContainer : public ECGGraphContainer, public SampleConsumer 
{
  Q_OBJECT

 public:

  /** Pass in a daq graph to become the container of.  Graph gets reparented
      to be a child of this class! */
  DAQECGGraphContainer(ECGGraph *graph, 
		       uint channelId,
		       QWidget *parent = 0, 
		       const char *name = 0, 
		       WFlags flags = 0);

  /* as per the SampleConsumer 'interface' */
  void consume(const SampleStruct *);

  uint channelId;

 signals:
  /* So that we can tell inquiring minds that want to know we are gone */
  void closing(const DAQECGGraphContainer *self);

  /* Emitted whenever the range changes on the graph this container 
     contains.  Range is the index of the range in this container's 
     private combo box.  Channelid comed fros the DAQECGGraph */
  void rangeChanged (uint channelId, int range);

 protected slots:
  void _rangeChangedWrapper(int range); /* signal wrapper 
					   handler for parent signal
					   to augment it with rangeChaned()
					   from this class */


 /* so that the status bar can let the user know what scan index
    this channel is up to */
  void setCurrentIndexStatus(uint64 index);

  /* Slot for updating the 'Mouse pos' status bar line.
     The 'index' we get here (from the ECGGraph class) is inaccurate
     as dropped samples can lead to de-synchronization between
     the graph and the actual sample's scan index.
     In addition we may also have ab O.B.1 (Obi-Wan) error here -- I didn't 
     check since I will re-write this mechanism soon.  --Calin */
  void setMouseVectorStatus(double voltage, uint64 index);

  void setSpikeThreshHoldStatus(double voltage);
  void unsetSpikeThreshHoldStatus();
  void setLastSpikeStatus(uint64 index, double voltage);

 protected:

  virtual void closeEvent(QCloseEvent *e); /* from QWidget */

  /* let's try this the Qt way (no automatic storage) */
  QStatusBar *statusBar;
  QLabel *currentIndex, *mouseOverVector, *spikeThreshHold, *lastSpike;

 private:
  uint64 last_scan_index;

};

#endif
