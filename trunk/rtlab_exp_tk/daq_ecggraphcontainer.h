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

#include "ecggraph.h"
#include "ecggraphcontainer.h"


class DAQECGGraphContainer : public ECGGraphContainer {
  Q_OBJECT

 public:

  /** Pass in a daq graph to become the container of.  Graph gets reparented
      to be a child of this class! */
  DAQECGGraphContainer(ECGGraph *graph, 
		       unsigned int channelId,
		       QWidget *parent = 0, 
		       const char *name = 0, 
		       WFlags flags = 0);


  unsigned int channelId() const { return  channel_id; }

 signals:
  /* emitted whenever this graph is closing */
  void closing (unsigned int channelId);

  /* emitted whenever the range changes on the graph this container 
     contains.  Range is the index of the range in this container's 
     private combo box.  Channelid comed from the DAQECGGraph */
  void rangeChanged (unsigned int channelId, int range);

 protected slots:
  void _rangeChangedWrapper(int range); /* signal wrapper 
					     handler for parent signal
					     to augment it with rangeChaned()
					     from this class */
 protected:

  virtual void closeEvent(QCloseEvent *e); /* from QWidget */
  
  unsigned int channel_id;
};

#endif
