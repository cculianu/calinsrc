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
#include <string.h>
#include "configuration.h"
#include "sample_source.h"
#include "sample_reader.h"
#include "sample_writer.h"
#include "ecggraph.h"
#include "ecggraphcontainer.h"
#include "simple_text_editor.h"
#ifdef __RTLINUX__
#endif

#define DAQ_SYSTEM_APPNAME_CSTRING "DAQSystem"

class DAQSystem;
class Plugin;

/* A tiling class whose slot, tyle(), can be used to replace the somewhat
   undesirably-implemented tile() in QWorkspace */
class Tyler: public QObject {
  Q_OBJECT
public:
  Tyler(QWorkspace *ws) : ws(ws){};
public slots:
  void tyle();
private:
  QWorkspace *ws;
};

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
  
  ReaderLoop(DAQSystem *daq_system);
  ~ReaderLoop();
   
 protected slots:
  /* this should be the target of a singleshot timer */
  void loop();
  void turnOffChannel(uint chan_id);

 private slots:
  void turnOffPending();

 private:
  vector<uint> pending_off;

 signals:
  void scanIndexChanged(scan_index_t index); /* emitted once per second,
                                                whenever we reach a new scan 
                                                index */
  void spikeDetected (const SampleStruct *);

 protected:

  DAQSystem *daq_system;

  bool graphListenerExists(uint channel_id);

  SampleStructSource *source, *spike_source;
  SampleStructReader *reader, *spike_reader;
  SampleWriter *writer;
  
  bool pleaseStop;

  uint n_channels; // redundant as n_channels == producers.size()

  /* producers[channel_index] = a producer that is producing for 
     the channel number equal to its index in the vector 
     (ie 'channel_index') */
  vector<Producer<const SampleStruct *> > producers;

  scan_index_t saved_curr_index;

  int last_sleep_time; /* used to indicate about how much we slept on 
                          the fifo last time */
};

class PluginMenu: public QWidget
{
  Q_OBJECT
public:
  PluginMenu(DAQSystem * daqSystem, 
             QWidget *parent = 0, const char * name = 0, WFlags = 0);
  ~PluginMenu();
  
public slots:
  void raisenShow();

private slots:
  bool queryLoadPlugin(); /* asks the user for a plugin to load       */
  void removeSelectedPlugin(); /* unloads and deletes the selected plugin */

  void pluginMenuContextReq(QListBoxItem *, const QPoint &);
  
  void unloadAll();

private:
  void loadPlugin(const char *filename) throw (Exception);
  void unloadPlugin(Plugin *p);

  map <Plugin *, int *> plugins_and_handles;
  QListBox *plugin_box;
  QPopupMenu *plugin_cmenu;

  DAQSystem * daqSystem; 
};

class DAQSystem : public QMainWindow
{
  Q_OBJECT

  friend class ReaderLoop;

 public:
  DAQSystem (ConfigurationWindow & cw, QWidget * parent = 0, 
             const char * name = DAQ_SYSTEM_APPNAME_CSTRING, 
             WFlags f = WType_TopLevel);

  ~DAQSystem();

  const ComediDevice & currentDevice() const { return currentdevice; }

  static bool isValidDAQSettings(const DAQSettings & s);

 public slots:
  void addChannel(); 
  void openChannelWindow(uint chan, uint range, uint n_secs);
  void saveGraphSettings(const ECGGraphContainer *self);
  void about() { /* about the application */ };

  /* Log-related slots */
  void logTimeStamp(); /* writes the current sample count into the log */

  
 protected slots:
  /* this slot does the necessary work to tell the rt-process that a 
     channel's range/gain setting needs to be changed */
  void graphChangedRange(uint channel, int range); 

  void setStatusBarScanIndex(scan_index_t index);

  /* window-related */
 public slots:
  int  windowMenuAddWindow(QWidget *w); /* returns window id */
  void windowMenuRemoveWindow(int window_id);
  /* focuses a window that has menu id 'id' in the windowMenu popup menu */
  void windowMenuFocusWindow(int id); 

  void spikePolarityChanged(SpikePolarity);
  void spikeBlankingChanged(uint);
  void spikeThresholdSet(double value);
  void spikeThresholdOff();

 private slots:

  void windowMenuRemoveWindow(const QWidget *w);
  /* defeat qt's type-dumbness with signals/slots */
  void windowMenuRemoveWindow(const ECGGraphContainer *w)
   { windowMenuRemoveWindow(static_cast<const QWidget *>(w)); }

  /* yet another really redundant slot:
     1) turns off the graph in the ShmMgr
     2) notifies the readerLoop */
  void graphOff(const ECGGraphContainer *);

  void resynch();

 protected:
  virtual void closeEvent(QCloseEvent *e); /* from QWidget */

  ConfigurationWindow & configWindow;
  DAQSettings & settings;
  const ComediDevice & currentdevice;

 private:
  bool queryOpen(uint & chan, uint & range, 
                 uint & n_secs);
  void buildRangeSettings(ECGGraphContainer *container);

  QWorkspace ws; 

  QMenuBar _menuBar;
  QPopupMenu fileMenu, logMenu, channelsMenu, windowMenu, helpMenu;

  /* keeps track of windowMenu indexes -> MDI windows so that the 
     Window menu works */
  map <int, QWidget *> menuIdToWindowMap; 

  QToolBar mainToolBar;
  QToolButton addChannelB,  resynchB, timeStampB;
  QStatusBar statusBar;
  QLabel statusBarScanIndex;
  ReaderLoop readerLoop;

  bool daqSystemIsClosingMode;

  SimpleTextEditor log; /* the experiment log window */
  Tyler tyler;
  
  PluginMenu plugin_menu;
};



#endif 