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

#ifndef _DAQ_SYSTEM_H
# define _DAQ_SYSTEM_H

#include <qmainwindow.h>
#include <qworkspace.h>
#include <qmenubar.h>
#include <qpopupmenu.h>
#include <qtoolbar.h>
#include <qtoolbutton.h>
#include <qbuttongroup.h>
#include <qstatusbar.h> 

#include <map>
#include <vector>
#include "string.h"
#include "configuration.h"
#include "sample_source.h"
#include "sample_reader.h"
#include "sample_writer.h"
#include "ecggraph.h"
#include "daq_ecggraphcontainer.h"
#include "log.h"
#ifdef __RTLINUX__
#endif


class DAQSystem;

/* This class does the nitty gritty reading of data off the fifo.
   This is a very comedi-specific, fifo source specific class.
   Approach probably needs to be rethought when I add more features 
   and/or datasource types to daq_system -cc 
 
   Threads weren't working so I reverted back to using one main thread */
class ReaderLoop: public QObject
{

  Q_OBJECT

  friend class DAQSystem;

 protected:
  
  ReaderLoop(uint n_channels, const DAQSettings & settings);
  ~ReaderLoop();
  
  
  /* adds a listener to the list for channels in listener->channelIds() */
  void addListener(SampleListener *listnener);

 protected slots:
  /* removes a listeners from the list*/
  void removeListener(const SampleListener *listener);
  /* removes only the listeners that concern themselves with graphing */
  void removeGraphListeners (uint chan);
  /* this should be the target of a singleshot timer */
  void loop();

 signals:
  void scanIndexChanged(scan_index_t index); /* emitted whenever we
						reach a new scan index */
  
 protected:

  bool isGraphListener(uint channel_id, uint index)
  {
    if (channel_id < listeners.size() && index < listeners[channel_id].size())
      return dynamic_cast<DAQECGGraphContainer*>(listeners[channel_id][index]);  
    return false;
  }

  bool graphListenerExists(uint channel_id);

  SampleStructSource *source;
  SampleStructReader *reader;
  SampleGZWriter *writer;
  
  bool pleaseStop;

  /* listeners[channel_index] = 
     a vector of sample listener pointers that  are interested in channel 
     'channel_index' */
  vector<vector<SampleListener *> > listeners;
};

class DAQSystem : public QMainWindow
{
  Q_OBJECT

 public:
  DAQSystem (ConfigurationWindow & cw, QWidget * parent = 0, 
	     const char * name = "DAQSystem", WFlags f = WType_TopLevel);

  ~DAQSystem();

  const ComediDevice & currentDevice() const { return currentdevice; }


 public slots:
  void addChannel(); 
  void openChannelWindow(uint chan, uint range, uint n_secs);
  void removeGraphContainer(const DAQECGGraphContainer *);
  void about() { /* about the application */ };

 protected slots:
  /* this slot does the necessary work to tell the rt-process that a 
     channel's range/gain setting needs to be changed */
  void graphChangedRange(uint channel, int range); 

  void setStatusBarScanIndex(scan_index_t index);

 private slots:

  /* focuses a window that has menu id 'id' in the windowMenu popup menu */
  void windowMenuFocusWindow(int id); 
  void windowMenuAddWindow(QWidget *w);
  void windowMenuRemoveWindow(const QWidget *w);
  /* stupid Qt needs exact type */
  void windowMenuRemoveWindow(const DAQECGGraphContainer *w) 
    {windowMenuRemoveWindow((const QWidget *)w); } 

 protected:
  virtual void closeEvent(QCloseEvent *e); /* from QWidget */

  ConfigurationWindow & configWindow;
  DAQSettings & settings;
  const ComediDevice & currentdevice;

 private:
  bool queryOpen(uint & chan, uint & range, 
		 uint & n_secs);
  void buildRangeSettings(ECGGraphContainer *contianer);

  QWorkspace ws;
  QMenuBar _menuBar;
  QPopupMenu fileMenu, channelsMenu, windowMenu, helpMenu;

  /* keeps track of windowMenu indexes -> MDI windows so that the 
     Window menu works */
  map <int, QWidget *> menuIdToWindowMap; 

  QToolBar mainToolBar;
  QToolButton addChannelB;
  QStatusBar statusBar;
  QLabel statusBarScanIndex;
  ReaderLoop readerLoop;

  /* keep track of the graph container windows we have 
     association is channel_id -> DAQECGGraphContainer * */
  map <uint, DAQECGGraphContainer *> gcontainers;
  bool daqSystemIsClosingMode;

  ExperimentLog log; /* the experiment log window */

};



#endif 
