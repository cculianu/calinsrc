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
#include <qlistview.h>
#include <qtextedit.h>
#include <qtextbrowser.h>

#include <map>
#include <vector>
#include <string.h>
#include "exception.h"
#include "configuration.h"
#include "producer_consumer.h"
#ifdef __RTLINUX__
#endif

#define DAQ_SYSTEM_APPNAME_CSTRING "DAQSystem"

class DAQSystem;
class Plugin;

class ECGGraph;
class ECGGraphContainer;

class  SampleStructSource;
class  SampleStructReader;
class  SampleWriter;
class  ShmController;
class  ShmBase;

class ExperimentLog; /* this extends class SimpleTextEditor, 
                        but it is an opaque type inside daq_system.cpp */

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

 protected:

  DAQSystem *daq_system;

  ShmController * shmCtl; /* daq_system accesses this */

  bool graphListenerExists(uint channel_id);

  SampleStructSource *source;
  SampleStructReader *reader;
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
  void showDetails(QListViewItem *);
  void carefullyLoadSelected(); /* asks the user for a plugin to load       */
  void showSelectedWindow();  /* focuses the selected plugin */
  void removeSelectedPlugin(); /* unloads and deletes the selected plugin */

  void pluginMenuContextReq(QListViewItem *, const QPoint &, int);
  
  void unloadAll();

private:

  struct PluginInfo {
    QString name;
    QString description;
    QString author;
    QString requires;
    QString short_filename; /* sans leading directory */
    QString filename; /* with leading directory */
  };

  typedef vector<PluginInfo> scanned_plugins_t;
  typedef scanned_plugins_t::iterator scanned_plugins_it_t;

  scanned_plugins_t scanned_plugins;

  scanned_plugins_it_t findScannedByName(const QString & name); 

  Plugin *pluginFindByName(QString name);

  void loadPlugin(const char *filename, Plugin * & p, void * & handle) 
    throw (PluginException);
  void unloadPlugin(Plugin *p);
  bool inspectPlugin(const char *filename, PluginInfo & info) const;

  map <Plugin *, int *> plugins_and_handles;
  QListView *plugin_box;
  QTextEdit *details;
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

  const ShmController & shmController() const { return shmCtl; }

 signals:
  void channelOpened(uint chan);
  void channelClosed(uint chan);
  
 public slots:
  void addChannel();  /* brings up the add channel dialog box */
  void changeAREFDialog();  /* brings up the change aref dialog box */
  void changeAREF(ComediChannel::AnalogRef); /* applies the aref change 
                                               to all channels */
  void openChannelWindow(uint chan, uint range, uint n_secs);
  void saveGraphSettings(const ECGGraphContainer *self);
  //void about();    /* about the application */
  void daqHelp( int id );

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
  void windowMenuFocusWindow(QWidget *w);
  void spikePolarityChanged(SpikePolarity);
  void spikeBlankingChanged(uint);
  void spikeThresholdSet(double value);
  void spikeThresholdOff();

  void resynch(); /* synchronize graph windows and tile them as well */
  void showLogWindow(); /* calls show() on the Log Window widget */
  void printDialog(); /* brings up various pring dialogs and the like */

 private slots:

  void windowMenuRemoveWindow(const QWidget *w);
  /* defeat qt's type-dumbness with signals/slots */
  void windowMenuRemoveWindow(const ECGGraphContainer *w);

  /* yet another really redundant slot:
     1) turns off the graph in the ShmMgr
     2) notifies the readerLoop */
  void graphOff(const ECGGraphContainer *);


 protected:
  virtual void closeEvent(QCloseEvent *e); /* from QWidget */

  ConfigurationWindow & configWindow;
  DAQSettings & settings;
  const ComediDevice & currentdevice;

 private:
  bool queryOpen(uint & chan, uint & range, 
                 uint & n_secs);
  void buildRangeSettings(ECGGraphContainer *container);

  /* Prints the current graph's contents for each of the graph containers
     specified in the vector */
  void print(vector<const ECGGraphContainer *> &);

  void allChannelsOffSaveState(vector<uint> & vector_to_save_ON_channels_to);
  void channelsOn (const vector<uint> & chanspec);

  QPrinter printer;  /* the printer used for printing */

  QWorkspace ws; 

  QMenuBar _menuBar;
  QPopupMenu fileMenu, logMenu, channelsMenu, windowMenu, helpMenu;

  /* keeps track of windowMenu indexes -> MDI windows so that the 
     Window menu works */
  map <int, QWidget *> menuIdToWindowMap; 
  /* Convenience method that returns the subset of the above widgets that are 
     ECGGraphContainers */
  vector<const ECGGraphContainer *> graphContainers() const;

  QToolBar mainToolBar;
  QToolButton addChannelB,  resynchB, timeStampB;
  QStatusBar statusBar;
  QLabel statusBarScanIndex;
  ReaderLoop readerLoop;
  ShmController & shmCtl; /* this comes directly from readerLoop */

  bool daqSystemIsClosingMode;

  ExperimentLog * log; /* the experiment log window */
  Tyler tyler;
  
  PluginMenu plugin_menu;

  enum HelpMenuIds { help1 = 0, help2, help3, n_help_dests };
  static const QString helpMenuDestinations[n_help_dests];
};

#endif 
