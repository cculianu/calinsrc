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
#include <qtoolbar.h>
#include <qtoolbutton.h>
#include <qbuttongroup.h>
#include <map>
#include "string.h"
#include "configuration.h"
#include "sample_source.h"
#include "sample_reader.h"
#include "ecggraph.h"
#include "daq_ecggraphcontainer.h"
#ifdef __RTLINUX__
#endif


/* This class exists as a convenience just to make some notation 
   a bit shorter.. :)  */
class DAQSystemGUIObject {
 public:
  enum ButtonOp {
    buttonop_toolow = -1,
    AddChannel,
    buttonop_toohigh
  };
};

class ButtonOpGroup: public QButtonGroup, public DAQSystemGUIObject
{
  Q_OBJECT
    
 public:

  ButtonOpGroup(QWidget *parent = 0, const char *name = 0);


 signals:
  void buttonOpClicked (ButtonOp op);

 public slots:
  /* converts the button id to a button op.. very trivial slot */
  void id2ButtonOp (int id);
};

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

  ReaderLoop(const ComediSubDevice & sdev, const DAQSettings & settings);
  ~ReaderLoop();
  
  
  class GraphList {
    friend class ReaderLoop;

    GraphList (unsigned int n_channels)  
      : n_graphs(n_channels)
      { 
	graphs = new DAQECGGraphContainer * volatile [n_channels];
        for (unsigned int i = 0; i < n_channels; graphs[i++] = 0);
      }

    ~GraphList () { if (graphs) delete graphs; n_graphs = 0; }

    DAQECGGraphContainer * volatile * volatile graphs;
    unsigned int n_graphs;    
  };

  /* adds a listening graph to the list for channel graph->channelId() */
  void addListeningGraph(DAQECGGraphContainer *graph);

 protected slots:
  /* remove a listening graph from the list for channel graph->channelId() */
  void removeListeningGraph(unsigned int chan);

 /* this should be the target of a singleshot timer */
  void loop();


 protected:
  /* true if the required channel has a graph listener  already
     false otherwise */     
  bool graphListenerExists(unsigned int channel_id)
    {
      if (channel_id < listeningGraphs.n_graphs 
	  && listeningGraphs.graphs[channel_id] )
	return true;
      return false;
    }

  const ComediSubDevice &sdev;
  SampleStructSource *source;
  SampleStructReader *reader;
  
  bool pleaseStop;

  /* graphList [channel_index] = a list of graph pointers that are interested 
     in channel 'channel_index' */
  GraphList listeningGraphs;
};

class DAQSystem : public QMainWindow, public DAQSystemGUIObject
{
  Q_OBJECT

 public:
  DAQSystem (ConfigurationWindow & cw, QWidget * parent = 0, 
	     const char * name = "DAQSystem", WFlags f = WType_TopLevel);

  ~DAQSystem();

  const ComediDevice & currentDevice() const { return currentdevice; }


 public slots:
  void channelOperation(ButtonOp op); 
  void openChannelWindow(unsigned int chan, unsigned int range, 
			 unsigned int n_secs);
  void removeGraphContainer(unsigned int chan);

 protected slots:
  /* this slot does the necessary work to tell the rt-process that a 
     channel's range/gain setting needs to be changed */
  void graphChangedRange(unsigned int channel, int range); 

 protected:
  virtual void closeEvent(QCloseEvent *e); /* from QWidget */

  ConfigurationWindow & configWindow;
  DAQSettings & settings;
  const ComediDevice & currentdevice;

 private:
  bool queryOpen(unsigned int & chan, unsigned int & range, 
		 unsigned int & n_secs);
  void buildRangeSettings(ECGGraphContainer *contianer);

  QWorkspace ws;
  QToolBar mainToolBar;
  QToolButton addChannel;
  ButtonOpGroup toolBarButtonGroup; /* invisible, pretty trivial widget */
  ReaderLoop readerLoop;

  /* keep track of the mdi windows we have */
  map <unsigned int, DAQECGGraphContainer *> gcontainers;
  bool daqSystemIsClosingMode;
};



#endif 
