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

#include <qapplication.h>
#include <qmainwindow.h>
#include <qworkspace.h>
#include <qbuttongroup.h>
#include <qtoolbar.h>
#include <qtoolbutton.h>
#include <qinputdialog.h> 
#include <qmessagebox.h>
#include <qtimer.h>

#include <iostream>
#include <map>
#include <vector>
#include <stdio.h>
#include "configuration.h"
#include "sample_source.h"
#include "sample_reader.h"
#include "sample_writer.h"
#include "shm_mgr.h"
#include "shared_stuff.h"
#include "ecggraph.h"
#include "daq_ecggraphcontainer.h"
#include "daq_system.h"

DAQSystem::DAQSystem (ConfigurationWindow  & cw, QWidget * parent = 0, 
		      const char * name = "DAQSystem", 
		      WFlags f = WType_TopLevel)
: QMainWindow(parent, name, f), 
  configWindow(cw),
  settings(configWindow.daqSettings()),
  currentdevice(cw.selectedDevice()),
  ws(this),
  _menuBar(this),
  mainToolBar("DAQ System Toolbar", this, Top),
  addChannelB(&mainToolBar),
  statusBar(this),
  statusBarScanIndex(&statusBar),
  //readerLoop(currentdevice.find(ComediSubDevice::AnalogInput).n_channels, 
  //     settings),
  readerLoop(this),
  daqSystemIsClosingMode(false), /* for now, this becomes true when
				    daq system is closing */
  log(&ws, "Experiment Log")
{
  this->setCentralWidget(&ws);
  ws.setScrollBarsEnabled(true);
  ws.show();

  statusBar.addWidget(&statusBarScanIndex);

  { /* build menus */
    fileMenu.insertItem("&Options", &configWindow, SLOT ( show() ) );
    fileMenu.insertSeparator();
    fileMenu.insertItem("&Quit", this, SLOT( close() ) );
    channelsMenu.insertItem("&Add Channel...", this, SLOT( addChannel() ) );
    windowMenu.insertItem("&Cascade", &ws, SLOT( cascade() ) );
    windowMenu.insertItem("&Tile", &ws, SLOT( tile() ) );
    windowMenu.insertSeparator(); /* after this, all open windows will be
				     stored */
    connect(&windowMenu, SIGNAL(activated(int)), 
	    this, SLOT(windowMenuFocusWindow(int)));

    helpMenu.insertItem("&About", this, SLOT( about() ) );

    _menuBar.insertItem("&File", &fileMenu);
    _menuBar.insertItem("&Channels", &channelsMenu);
    _menuBar.insertItem("&Window", &windowMenu);
    _menuBar.insertSeparator();
    _menuBar.insertItem("&Help", &helpMenu);
  }

  /* add the log window */
  log.useTemplate(settings.getTemplateFileName());
  windowMenuAddWindow(&log); /* add this window to the windowMenu QPopupMenu */

  { /* add toolbar */
    addChannelB.setText("Add Channel...");
    connect(&addChannelB, SIGNAL(clicked()), this, SLOT (addChannel()));
    
    setDockEnabled(Top, true);   setDockEnabled(Bottom, true);
    setDockEnabled(Left, true);  setDockEnabled(Right, true);
    setDockMenuEnabled(true);
    setToolBarsMovable(true);
    addToolBar(&mainToolBar);
  }

  connect(&readerLoop, SIGNAL(scanIndexChanged(scan_index_t)),
	  this, SLOT(setStatusBarScanIndex(scan_index_t)));
  
  /* now open up all the channel windows that are left over
     from the last session */
  set<uint> s = settings.windowSettingChannels();
  for (set<uint>::iterator i = s.begin(); i != s.end();  i++) {
    openChannelWindow(*i, 0, 10); /* modify this to read other defaults too */
  }
  
  /* now begin the main loop that grabs data and plots it */
  readerLoop.loop();
  
}

DAQSystem::~DAQSystem()
{
  settings.saveSettings(); /* commit settings (usually just window setting
			      changes) */
}

void
DAQSystem::addChannel (void)
{  
  bool ok;
  uint chan, range, n_secs;

  ok = queryOpen(chan, range, n_secs);
  if (ok) {
    openChannelWindow(chan, range, n_secs);
  }
}

bool
DAQSystem::queryOpen(uint & chan, uint & range, 
		     uint & n_secs)
		     
{
  static const uint n_seconds_options[] = { 5, 10, 15, 20, 25, 30 };
  /* this remembers the last dialog interaction */
  static uint prevRange = 0, prevNSecs = 0;
  bool retval;
  map<uint, uint> cbox2idMap,id2cboxMap;

  QDialog openDialog(this, 0, true);
  QGridLayout gl(&openDialog, 7, 2); /* cheap and easy layout */
  QLabel 
    enterId("Choose a channel to monitor", &openDialog),
    enterGain("Choose a gain setting for this channel", &openDialog),
    enterNSecs("Choose the number of seconds to display", &openDialog);
  QComboBox 
    id(false, &openDialog), 
    gain(false, &openDialog), 
    nSecs(false, &openDialog);
  QPushButton ok("Ok", &openDialog), cancel("Cancel", &openDialog);
  
  gl.addMultiCellWidget(&enterId, 0, 0, 0, 1);
  gl.addMultiCellWidget(&id, 1, 1, 0, 1);
  gl.addMultiCellWidget(&enterGain, 2, 2, 0, 1);
  gl.addMultiCellWidget(&gain, 3, 3, 0, 1);
  gl.addMultiCellWidget(&enterNSecs, 4, 4, 0, 1);
  gl.addMultiCellWidget(&nSecs, 5, 5, 0, 1);
  gl.addMultiCellWidget(&ok, 6, 6, 0, 0);
  gl.addMultiCellWidget(&cancel, 6, 6, 1, 1);
  connect(&ok, SIGNAL(clicked(void)), &openDialog, SLOT(accept(void)));
  connect(&cancel, SIGNAL(clicked(void)), &openDialog, SLOT(reject(void)));
  { /* build combo boxex */
    const ComediSubDevice & subdev 
      = currentDevice().find(ComediSubDevice::AnalogInput);
    for (uint i = 0, cboxid = 0; 
	 i < (uint)subdev.n_channels ||
	   i < subdev.ranges().size() ||
	   i < sizeof(n_seconds_options) / sizeof(const int);
	 i++) {
      
      if (i < (uint)subdev.n_channels 
	  && ! readerLoop.producers[i].findByType((DAQECGGraphContainer *)0)) {
	/* build channel id combo box inside here */
	id.insertItem("Channel No. " + QString().setNum(i));
	cbox2idMap [ cboxid ] = i;
	id2cboxMap [ i ] = cboxid;
	cboxid++;
      }
      if (i < subdev.ranges().size() ) {
	/* build range id combo box inside here */
	const comedi_range & r = subdev.ranges()[i];
	QString u( ( r.unit == UNIT_volt ? "V" : "mV" ) );
	gain.insertItem(QString().setNum(r.min) + u + " - " 
			+ QString().setNum(r.max) + u);
      }
      if (i < sizeof(n_seconds_options) / sizeof(const int)) {
	/* build n_secs combo box here */
	nSecs.insertItem(QString().setNum(n_seconds_options[i]) +" seconds");
      }
    }
    
  }

  /* validate that we can proceed... */
  if (id.count() <= 0) {
    /* barf our early with warning if they already have all windows
       open */
    QMessageBox::information(this, "Cannot open another channel window",
			     "Cannot open another channel window "
			     "as all channels are being graphed already!");
    return false;
  }

  /* set defaults from last time */
  gain.setCurrentItem(prevRange);
  for (uint i = 0; 
       i < sizeof(n_seconds_options) / sizeof(const int); i++) {
    /* find the correct n-seconds index */
    if (n_seconds_options[i] == prevNSecs)
      nSecs.setCurrentItem(i);
  }

  openDialog.exec();
  
  retval =  openDialog.result() == QDialog::Accepted;
  if (retval) {
    chan = cbox2idMap[ id.currentItem() ];
    range = gain.currentItem();
    n_secs = n_seconds_options[nSecs.currentItem()];    
    /* save ok settings on accepted */
    prevRange = range; prevNSecs = n_secs;
  }
  return retval;
}

void
DAQSystem::openChannelWindow(uint chan, uint range, 
			     uint n_secs)
{
  
  QRect pos(settings.getWindowSetting(chan)); /* grab window pos from file */

  /* leaky memory prevented by spaghetti-like destructive close
     on the DAQECGGraphContainer and a signal-emitting
     destructor in DAQECGGraph */
  ECGGraph *graph = new ECGGraph(1000, n_secs);

  /* todo: put the below in a deleteable place! */
  DAQECGGraphContainer *gcont =
    new DAQECGGraphContainer(graph, chan, &ws, 
			     QString("Channel %1").arg(chan).latin1());

  buildRangeSettings(gcont);

  windowMenuAddWindow(gcont); /* add this window to the 
				 windowMenu QPopupMenu */

  gcont -> show(); /* will this help with window setting/geometry? */
  readerLoop.producers[chan].add(gcont);
  /* see about getting rid of these signal/slots and use natural signalling
     in producer/consumer classes.. -CC */
  connect(gcont, SIGNAL(closing(const DAQECGGraphContainer *)),
	  this, SLOT(saveGraphWindowPositions(const DAQECGGraphContainer *)));
  connect(gcont, SIGNAL(closing(const DAQECGGraphContainer *)),
	  this, SLOT(windowMenuRemoveWindow(const DAQECGGraphContainer *)));
  connect(gcont, SIGNAL(rangeChanged(uint, int)),
	  this, SLOT(graphChangedRange(uint, int)));

  gcont->rangeChange( range ); /* hopefully above slot will take effect 
				  and update rt_process appropriately? */

  /* ladies and gentlemen, start your samplings!! */
  ShmMgr::setChannel(ComediSubDevice::AnalogInput, chan, true);

  if (! pos.isNull() ) { /* means we had a default setting */
    gcont->show();
    gcont->setGeometry(pos);
    gcont->move(pos.x(), pos.y());
  }

}

/* this slot can be called from either the closeEvent of
   the DAQECGGraphContainers, or from the closeEvent of
   the DAQSystem classes. It behaves slightly differently
   depending on who calls it. (daqSystemIsClosingMode is different). 
   Purpose is to commit graph container window settings to settings file, 
   or flush them from the settings file if they weren't open
   upon application close */
void
DAQSystem::saveGraphWindowPositions(const DAQECGGraphContainer * gcont)
{
  uint chan = gcont->channelId; 

  if (daqSystemIsClosingMode) {
    /* save actual window settings */
    QRect pos(gcont->geometry());
    pos.setX(gcont->pos().x()); pos.setY(gcont->pos().y());
    settings.setWindowSetting(chan, pos);
  } else {
    /* clear window settings to indicte this window should not be auto-opened
       the next time */
    settings.setWindowSetting(chan, QRect());
  }

  ShmMgr::setChannel(ComediSubDevice::AnalogInput, chan, false);
}

void 
DAQSystem::closeEvent (QCloseEvent * e)
{
  daqSystemIsClosingMode = true; /* this changes the behavior of the
				    saveGraphWindowPositions() method */
  for (uint i = 0; i < readerLoop.producers.size(); i++) {
    const DAQECGGraphContainer *gc;
    if ( (gc = readerLoop.producers[i].findByType((DAQECGGraphContainer *)0)) )
      saveGraphWindowPositions(gc);
  }
  QMainWindow::closeEvent(e);
}

/* this slot does the necessary work to tell the rt-process that a 
     channel's range/gain setting needs to be changed */
void
DAQSystem::
graphChangedRange(uint channel, int range)
{
  ShmMgr::setChannelRange(ComediSubDevice::AnalogInput, channel, 
			  (uint)range);
}

void
DAQSystem::windowMenuFocusWindow(int id)
{
  if (menuIdToWindowMap.find(id) != menuIdToWindowMap.end()) {
    menuIdToWindowMap[ id ]->show();     /* just in case its hidden */
					   

    menuIdToWindowMap[ id ]->setFocus(); /* is this how we 
					    activate it? */
  }
}

void
DAQSystem::windowMenuAddWindow(QWidget *w)
{
  /* now add this window to our 'Window' QPopupMenu */
  menuIdToWindowMap [ windowMenu.insertItem(w->name()) ] = w;
  /* /add window */
}

void
DAQSystem::windowMenuRemoveWindow(const QWidget *w)
{
  map<int, QWidget *>::iterator i;
  for (i = menuIdToWindowMap.begin(); i != menuIdToWindowMap.end(); i++) {
    if (i->second == w) {
      windowMenu.removeItem(i->first);
      menuIdToWindowMap.erase(i);
      return;
    }
  }
}

/* populates the graphcontianer with the correct range options for this 
   channel */
void 
DAQSystem::buildRangeSettings(ECGGraphContainer *c) 
{
  
  const vector<comedi_range> ranges = 
    currentDevice().find(ComediSubDevice::AnalogInput).ranges();

  for (uint i = 0; i < ranges.size(); i++) {
    c->addRangeSetting(ranges[i].min, ranges[i].max,
		       ( ranges[i].unit == UNIT_volt 
			 ? ECGGraphContainer::Volts
			 : ECGGraphContainer::MilliVolts ));
  }

}


void
DAQSystem::setStatusBarScanIndex(scan_index_t index)
{
  statusBarScanIndex.setText(QString("Global Scan Index: %1").arg((ulong)index));
}


ReaderLoop::
/* very comedi-specific constructor! Must change! */
ReaderLoop(DAQSystem *d) :
  daq_system(d), 
  pleaseStop(false), 
  n_channels(d->currentdevice.find(ComediSubDevice::AnalogInput).n_channels),
  producers(n_channels)
{
  switch (d->settings.getInputSource()) {
  case DAQSettings::RTProcess:
    ShmMgr::check();
    source = new SampleStructRTFSource();
    source->flush();
    /* build a non-blocking sample reader */
    reader = new SampleStructReader(source, 0);
    writer = new SampleGZWriter(d->settings.getDataFile().latin1());
    { /* setup the writer to listen */
      uint *tmp = new uint [n_channels];
      for (uint i = 0; i < n_channels; i++) {
	tmp[i] = i;
      }
      //writer->setChannelIds(tmp, n_channels);
      delete tmp;

    }
    // add the writer as a consumer for all channels
    for (uint i = 0; i < n_channels; i++) producers[i].add(writer);

    break;
  default:
    throw UnimplementedException 
      ("Unimplemented feature",
       "The use of non-rt_process input sources is not yet implemented!\n"
       "Either insmod rt_process.o or give up for now... (Sorry!)");
    break;
  }
}

ReaderLoop::~ReaderLoop()
{
  pleaseStop = true;
  /* fprintf here because streams seem to not handle long longs 
     properly? */
  fprintf (stderr,
	   "Read: %llu samples without errors, dropped %llu samples.\n%s",
	   reader->numRead(), reader->numDropped(),
	   (reader->numDropped()
	    ? "(Dropped samples can occur when the GUI task is too slow"
	       " for the\nReal-Time task, or when channels are turned off"
               " then back on, so\n that samples are skipped.)\n"
	    : ""));
  delete reader; reader = 0;
  delete source; source = 0;
  delete writer; writer = 0;
}

void
ReaderLoop::loop()
{
  
  if (pleaseStop) return;

  /* the following readAll()  may (theoretically) block indefinitely which is 
     why eventually we should think about threading this bad boy... :) */
  
  const SampleStruct *sbuf = reader->readAll(); 

  int n_read = reader->numLastRead();
  uint chan;

  for (int i = 0; i < n_read; i++) {    
    if ( (chan = sbuf[i].channel_id) < producers.size() ) {
      producers[chan].produce(sbuf + i);
    }
  }
  
  { /* emit scan index update */
    static scan_index_t saved_curr_index = 0;
    scan_index_t si = ShmMgr::scanIndex();
    if (saved_curr_index != si) {
      saved_curr_index = si;
      emit scanIndexChanged(saved_curr_index);
    }
  }

  QTimer::singleShot(source->suggestPollWaitTime(), 
		     this, SLOT(loop()));
}
