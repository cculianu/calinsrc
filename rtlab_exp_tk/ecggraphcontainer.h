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
#include <qlayout.h>
#include <qvbox.h>
#include <qhbox.h>
#include <qlabel.h>
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
                    scan_index_t scanIndexStatusIncrement = 1000,
                    uint seconds_visible_step = 5);
  
  virtual ~ECGGraphContainer();

  ECGGraph *graph;
  uint channelId;

  enum RangeUnit { /* units used in addRangeSetting */
    Volts,
    MilliVolts
  };

  /* as per the SampleConsumer 'interface' */
  void consume(const SampleStruct *);

  /** this populates the combobox with a string of the right
      format and the correct units */
  int addRangeSetting(double min, double max, RangeUnit unit);

  /** Deletes a range setting from the range combo box at the specified index 
      Returns false if index is invalid/not found */
  bool deleteRangeSetting(int index);

  /** returns index of the specified range setting in the combo box control, 
      or -1 if the setting is not found in the range combo box */
  int findRangeSetting(double rangeMin, double rangeMax, RangeUnit unit);

  /* the index of the current range setting.. basically retrieved from
     the gui widget that controls the range settings */
  int currentRange() const { return rangeComboBox->currentItem(); };
  const QString currentRangeText() const { return rangeComboBox->currentText(); };

  /** an array of strings to translate Volts and MilliVolts constants
      into pretty strings for display: 
      Ex. usage: QString volts = ECGGraphContainer::unitStrings[Volts]; */
  static const QString unitStrings[];

  uint secondsVisibleStepping() const { return seconds_visible_step; }
  void setSecondsVisibleStepping(uint s) { 
    if (s) {
      seconds_visible_step = s;
      setSecondsVisible(static_cast<uint>(secondsVisibleBox->value()));
    }
  }

  /* returns the xAxis strings as they appeaer in the graph container's
     x axis labels */
  vector<QString> xAxisText() const;
  
  bool isPaused() const { return nothungry; } //{ return pauseBox->isDown(); }

 signals:
  /* Emitted whenever the range changes on the graph this container 
     contains.  Range is the index of the range in this container's 
     private combo box.  Channelid comed fros the ECGGraph */
  void rangeChanged( uint channelId, int newIndex );
  void spikePolarityChanged(SpikePolarity);
  void spikeBlankingChanged(uint);

  /* So that we can tell inquiring minds that want to know we are gone */
  void closing(ECGGraphContainer *self);
  void closing(const ECGGraphContainer *self);

public slots:
  /** Custom slot...
      For use with combo-boxes that contain descriptive text of the new ranges.
      The rangeString should have the form "*xx.xx*yy.yy*" where 
      xx.xx and yy.yy are string representations of the floats for min
      and max ranges, respectively.
  */
  void rangeChange( const QString &rangeString ); 
  void rangeChange( int index );
  void rangeChange(double rangeMin, double rangeMax, RangeUnit unit);

  /** slot that is normally triggered from the rangeChanged(double, double)
      signal in the ECGGraph instance bound to this object instance */
  void updateYAxisLabels(double rangeMin, double rangeMax);

  void setSpikePolarity(SpikePolarity p)
    {spikePolarityButtons->setButton(p);};

  void setSpikeBlanking(int b)
    {spikeBlanking->setValue(b);}

  void setSecondsVisible(int);

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

  void pause(); /* pauses the graph 
                                    -- 'disables' it as a consumer */

 protected:

  virtual void closeEvent(QCloseEvent *e); /* from QWidget */

 private slots:
  void mouseUpInGraph();
  void mouseDownInGraph();

  void emitSpikeBlanking(int i) { emit spikeBlankingChanged(static_cast<uint>(i)); }
  void emitSpikePolarity(int i) 
    { emit spikePolarityChanged(static_cast<SpikePolarity>(i)); }

  void setXAxisLabels(const vector<uint64> &);

 private:
  
  QGridLayout *layout;

  /* parses 'string' for range settings and puts the results
     inside rangeMin, rangeMax, and units */
  bool parseRangeString(const QString &string, 
                        double & rangeMin, 
                        double & rangeMax, 
                        RangeUnit & units);
 

  QLabel *topYLabel, *middleYLabel, *bottomYLabel, *graphNameLabel;

  QPushButton *pauseBox;

  QVBox *labelBox; // for the y axis labels
 
  QHBox *controlsBox; // for the controls above the graph

  QComboBox *rangeComboBox;  // holds range labels
  QSpinBox *secondsVisibleBox; // holds/changes the number of seconds visible
  uint seconds_visible_step; /* defaults to 5 */
  QLabel *spikePolarityLabel;
  QButtonGroup *spikePolarityButtons;
  QRadioButton *polarityPlusButton, *polarityMinusButton;
  QSpinBox *spikeBlanking;

  /* status bar related stuff */
 QStatusBar *statusBar;
 QWidget *xaxis;
 QGridLayout *xaxis_layout;
 vector<QLabel *> xaxis_labels;
 vector<uint64> saved_sample_indices;

 QLabel *currentIndex, *mouseOverVector, *spikeThreshold, *lastSpike, 
         *spikeFrequency;

 
  const scan_index_t scan_index_threshold; // internal
  scan_index_t last_scan_index, 
               last_spike_index; /* for computing instantaneous hz */

};

#endif
