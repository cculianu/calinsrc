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
#ifndef _DAQ_GRAPH_CONTROLS_H
#  define _DAQ_GRAPH_CONTROLS_H


#include <map>
#include <vector>

#include <qtoolbar.h>
#include <qstring.h>

#include "common.h"
#include "comedi_device.h"
#include "daq_channel_params.h"
#include "spike_polarity.h"


class  QMainWindow;
class  QWidget;
class  QWorkspace;
class  QComboBox;
class  QSpinBox;
class  QButtonGroup;
class  QRadioButton;
class  QSpinBox;
class  QLabel;
class  QPushButton;

class ECGGraphContainer;

class DAQGraphControls : public QToolBar
{
  Q_OBJECT

 public:
  
  /* mainWindow should have a QWorkSpace as it's central widget! :) */
  DAQGraphControls ( const QString & label, QMainWindow * mainWindow,
                     QWorkspace *, QWidget * parent, bool newLine = FALSE, 
                     const char * name = 0, WFlags f = 0 );

  ~DAQGraphControls();


  uint addGraph ( const QString & label, ECGGraphContainer * g, 
                  const ComediSubDevice & dev, 
                  int id = -1 /* means auto-assign id */ );
  uint addGraph ( ECGGraphContainer *g, const DAQChannelParams & p);

  const ECGGraphContainer * currentGraph() const;
        ECGGraphContainer * currentGraph();

  uint currentGraphId() const;

  void activateGraph( uint id );
  void activateGraph( ECGGraphContainer * );

  const DAQChannelParams & getParams(uint id) const;
  const DAQChannelParams & getParams(const ECGGraphContainer *) const;

 signals:
  
  void rangeChanged( uint id, const QString &rangeString );
  void rangeChanged( uint id, uint comediRangeId );
  void rangeChanged( uint id, double min, double max );
  void secondsVisibleChanged( uint id, uint value );
  void spikeBlankingChanged( uint id, uint ms );
  void spikePolarityChanged( uint id, SpikePolarity p );
  void pausedUnpaused(uint id, bool paused);
  
 public slots:    

  void removeGraph ( const QString & label );
  void removeGraph ( const ECGGraphContainer * g );
  void removeGraph ( uint id );
 
  void setRange( int index ); 
  void setSecondsVisible( int value );
  void setSpikeBlanking( int ms );
  void toggleSpikePolarity();
  void setSpikePolarity(SpikePolarity p);
  void setSpikePolarityButton(SpikePolarity p); // just changes the button
  void togglePause();
  void setPausePlayButton(bool paused);


  void windowActivated(QWidget *);
  

 private:

  void init ();
  void sti(); /* setup (start) the signals that come to the member slots */
  void cli(); /* clear (stop) the signals that go to the member slots */
  void constructGUIs(); // builds the GUI elements to this ToolBar...
  void disableGUIs(); // greys out relevant widgets if we have no windows
  void enableGUIs(); // ungreys relevant widgets when we have windows
  void populateGUIs();
  void populateRangeComboBox();

  struct GraphParam : public DAQChannelParams {
    ECGGraphContainer *graph;
   
    GraphParam(const ComediSubDevice *d = 0) : DAQChannelParams(d), graph(0){};
    GraphParam(const DAQChannelParams & d): DAQChannelParams(d), graph(0) {};
  };

  static const int 
    secsVisibleStep,
    minSecsVisible,
    maxSecsVisible,
    minSpikeBlanking,
    maxSpikeBlanking,
    spikeBlankingStep;
  

  typedef map<uint, GraphParam> GraphParams;

  mutable GraphParams graphs;
  uint current; /* the current graph's id -- useful for dereferencing map */
  static const uint invalid_current;

  /* need actual GUI elements here */
  QLabel *graphNameLabel;
  QComboBox *rangeComboBox;  // holds range labels
  QSpinBox *secondsVisibleBox; // holds/changes the number of seconds visible
  QPushButton *pauseButton, *polarityButton;
  QSpinBox *spikeBlanking;

  typedef vector<QWidget *> WidgetVector;
  WidgetVector guisToDisableEnable;
  QWorkspace *ws;
};


#endif
