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
#ifndef __ECGGRAPHCONTAINER_H
#  define __ECGGRAPHCONTAINER_H

#include <qframe.h>
#include <qevent.h>
#include <qcombobox.h>
#include <qspinbox.h>
#include <qbuttongroup.h>
#include <qradiobutton.h>
#include <qstatusbar.h>
#include <qcheckbox.h>
#include <qpushbutton.h>
#include <qiconset.h>

#include <vector>

#include "common.h"
#include "ecggraph.h"
#include "sample_consumer.h"
#include "shared_stuff.h"

class QGridLayout;
class QVBox;
class QHBox;
class QLabel;
class QStatusBar;


class ECGGraphContainer : public QFrame, public SampleConsumer {
  Q_OBJECT

public:

  /** Pass in a graph to become the container of.  Graph gets reparented
      to be a child of this class! */
  ECGGraphContainer(ECGGraph *graph, 
                    uint channelId,
                    QWidget *parent = 0, 
                    const char *name = 0, 
                    WFlags flags = 0,
                    scan_index_t scanIndexStatusIncrement = 1000);
  
  ECGGraph *graph;
  uint channelId;

  /* as per the SampleConsumer 'interface' */
  void consume(const SampleStruct *);

  /* returns the xAxis strings as they appeaer in the graph container's
     x axis labels */
  vector<QString> xAxisText() const;
  
  bool isPaused() const { return nothungry; } //{ return pauseBox->isDown(); }

 signals:

  /* So that we can tell inquiring minds that want to know we are gone */
  void closing(ECGGraphContainer *self);
  void closing(const ECGGraphContainer *self);

public slots:
  /** slot that is normally triggered from the rangeChanged(double, double)
      signal in the ECGGraph instance bound to this object instance */
  void setYAxisLabels(double rangeMin, double rangeMax);

  /** needed to update labels */
  void detectSpike(const SampleStruct *);

 /* so that the status bar can let the user know what scan index
    this channel is up to */
  void setCurrentIndexStatus(scan_index_t index);

  /* Slot for updating the 'Mouse pos' status bar line.
     The 'index' we get here (from the ECGGraph class) is inaccurate
     as dropped samples can lead to de-synchronization between
     the graph and the actual sample's scan index.
     In addition we may also have ab O.B.1 (Obi-Wan) error here -- I didn't 
     check since I will re-write this mechanism soon.  --Calin */
  void setMouseVectorStatus(double voltage, uint64 index);

  void setSpikeThresholdStatus(double voltage);
  void unsetSpikeThresholdStatus();

  void pauseUnpause(); /* pauses the graph 
                          -- 'disables' it as a consumer */

  void setXAxis(bool on);
  void setYAxis(bool on);
  void setStatusBar(bool on);

 protected:

  virtual void closeEvent(QCloseEvent *e); /* from QWidget */

 private slots:
  void mouseUpInGraph();
  void mouseDownInGraph();

  void setXAxisLabels(const vector<uint64> &);

 private:
  
  QGridLayout *layout;

  QLabel *topYLabel, *middleYLabel, *bottomYLabel, *xaxisLabel;

  QVBox *labelBox; // for the y axis labels
 
  /* status bar related stuff */
  QStatusBar *statusBar;
  QWidget *xaxis;
  QGridLayout *xaxis_layout;
  vector<QLabel *> xaxis_labels;
  vector<uint64> saved_sample_indices;
  
  QLabel *spikeThreshold, *lastSpike, *spikeFrequency;
  
  
  const scan_index_t scan_index_threshold; // internal
  scan_index_t last_scan_index, 
               last_spike_index; /* for computing instantaneous hz */
};

#endif
