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
#include <stdio.h>
#include "configuration.h"
#include "sample_source.h"
#include "sample_reader.h"
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
  mainToolBar("DAQ System Toolbar", this, Top),
  addChannel(&mainToolBar),
  toolBarButtonGroup(this),
  readerLoop(currentdevice.find(ComediSubDevice::AnalogInput), settings),
  daqSystemIsClosingMode(false) /* for now, this becomes true when
					daq system is closing */
{
  this->setCentralWidget(&ws);
  ws.show();
  addChannel.setText("Add Channel...");
  toolBarButtonGroup.insert(&addChannel, AddChannel);
  toolBarButtonGroup.hide();
  connect(&toolBarButtonGroup, 
	  SIGNAL(buttonOpClicked(ButtonOp)),
	  this, SLOT (channelOperation(ButtonOp)));
		
  setDockEnabled(Top, true);   setDockEnabled(Bottom, true);
  setDockEnabled(Left, true);  setDockEnabled(Right, true);
  setDockMenuEnabled(true);
  setToolBarsMovable(true);
  addToolBar(&mainToolBar);


  /* empty menubar on the fly? */
  menuBar();

  /* now open up all the channel windows that are left over
     from the last session */
  set<unsigned int> s = settings.windowSettingChannels();
  for (set<unsigned int>::iterator i = s.begin(); i != s.end();  i++) {
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
DAQSystem::channelOperation (ButtonOp op)
{  
  bool ok;
  unsigned int chan, range, n_secs;

  switch (op) {
  case AddChannel:
    ok = queryOpen(chan, range, n_secs);
    if (ok) {
      openChannelWindow(chan, range, n_secs);
    }
    break;
  default:
    break;    
  }
}

bool
DAQSystem::queryOpen(unsigned int & chan, unsigned int & range, 
		     unsigned int & n_secs)
		     
{
  static const unsigned int n_seconds_options[] = { 5, 10, 15, 20, 25, 30 };
  /* this remembers the last dialog interaction */
  static unsigned int prevRange = 0, prevNSecs = 0;
  bool retval;
  map<unsigned int, unsigned int> cbox2idMap,id2cboxMap;

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
    for (unsigned int i = 0, cboxid = 0; 
	 i < (unsigned int)subdev.n_channels ||
	   i < subdev.ranges().size() ||
	   i < sizeof(n_seconds_options) / sizeof(const int);
	 i++) {
      
      if (i < (unsigned int)subdev.n_channels 
	  && ! readerLoop.graphListenerExists(i)) {
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
  for (unsigned int i = 0; 
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
DAQSystem::openChannelWindow(unsigned int chan, unsigned int range, 
			     unsigned int n_secs)
{

  QRect pos(settings.getWindowSetting(chan)); /* grab window pos from file */

  /* leaky memory prevented by spaghetti-like destructive close
     on the DAQECGGraphContainer and a signal-emitting
     destructor in DAQECGGraph */
  ECGGraph *graph = 
    new ECGGraph(1000, n_secs);

  graph->setName((const char *)("Channel " + QString().setNum(chan)).latin1());

  /* todo: put the below in a deleteable place! */
  DAQECGGraphContainer *gcont =
    new DAQECGGraphContainer(graph, chan, &ws, 0);
  buildRangeSettings(gcont);
  gcont -> show(); /* will this help with window setting/geometry? */
  readerLoop.addListeningGraph(gcont);
  connect(gcont, SIGNAL(closing(unsigned int)),
	  &readerLoop,SLOT(removeListeningGraph(unsigned int)));
  connect(gcont, SIGNAL(closing(unsigned int)),
	  this, SLOT(removeGraphContainer(unsigned int)));
  connect(gcont, SIGNAL(rangeChanged(unsigned int, int)),
	  this, SLOT(graphChangedRange(unsigned int, int)));

  gcont->rangeChange( range ); /* hopefully above slot will take effect 
				  and update rt_process appropriately? */

  /* ladies and gentlemen, start your samplings!! */
  ShmMgr::setChannel(ComediSubDevice::AnalogInput, chan, true);

  if (! pos.isNull() ) { /* means we had a default setting */
    gcont->show();
    gcont->setGeometry(pos);
    gcont->move(pos.x(), pos.y());
  }
  gcontainers [ chan ] = gcont;
}

/* this slot can be called from either the closeEvent of
   the DAQECGGraphContainers, or from the closeEvent of
   the DAQSystem classes. It behaves slightly differently
   depending on who calls it. (daqSystemIsClosingMode is different). */
void
DAQSystem::removeGraphContainer(unsigned int chan)
{
  QRect pos;

  if ( daqSystemIsClosingMode ) {
    DAQECGGraphContainer * gcont = gcontainers.find(chan)->second;
    if (gcont) {
      pos = (gcont->geometry());
      pos.setX(gcont->pos().x()); pos.setY(gcont->pos().y());
    }
  }
  
  settings.setWindowSetting(chan, pos);
  ShmMgr::setChannel(ComediSubDevice::AnalogInput, chan, false);
  gcontainers.erase(chan);

}

void 
DAQSystem::closeEvent (QCloseEvent * e)
{
  daqSystemIsClosingMode = true; /* this changes the behavior of the
				    removeGraphContainer() method */
  map<unsigned int, DAQECGGraphContainer *>::iterator i;
  for (i = gcontainers.begin(); i != gcontainers.end(); i++) {    
    removeGraphContainer(i->first); /* this saves window settings for each
				       open graph */
  }
  QMainWindow::closeEvent(e);
}

/* this slot does the necessary work to tell the rt-process that a 
     channel's range/gain setting needs to be changed */
void
DAQSystem::
graphChangedRange(unsigned int channel, int range)
{
  ShmMgr::setChannelRange(ComediSubDevice::AnalogInput, channel, 
			  (unsigned int)range);
}

/* populates the graphcontianer with the correct range options for this 
   channel */
void 
DAQSystem::buildRangeSettings(ECGGraphContainer *c) 
{
  
  const vector<comedi_range> ranges = 
    currentDevice().find(ComediSubDevice::AnalogInput).ranges();

  for (unsigned int i = 0; i < ranges.size(); i++) {
    c->addRangeSetting(ranges[i].min, ranges[i].max,
		       ( ranges[i].unit == UNIT_volt 
			 ? ECGGraphContainer::Volts
			 : ECGGraphContainer::MilliVolts ));
  }

}

ButtonOpGroup::ButtonOpGroup(QWidget *parent = 0, const char *name = 0)
  : QButtonGroup(parent, name) 
{
  connect(this, SIGNAL(clicked(int)), this, SLOT(id2ButtonOp(int)));
}

void
ButtonOpGroup::id2ButtonOp (int id)
{
  if (id > buttonop_toolow && id < buttonop_toohigh) {
    emit(buttonOpClicked((ButtonOp)id));
  } else {
    cerr << "WARNING: ButtonOpGroup::id2ButtonOp called with invalid "
            "button operation " << id << endl;
  }
}

ReaderLoop::
ReaderLoop(const ComediSubDevice & sdev, const DAQSettings & settings)
  : sdev(sdev), pleaseStop(false), 
    listeningGraphs(sdev.n_channels) /* very comedi-specific constructor! */
{
  switch (settings.getInputSource()) {
  case DAQSettings::RTProcess:
    ShmMgr::check();
    source = new SampleStructRTFSource();
    source->flush();
    /* build a non-blocking sample reader */
    reader = new SampleStructReader(source, 0);
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
}

void
ReaderLoop::loop()
{

  if (pleaseStop) return;
  
  /* the following readAll()  may block indefinately which is why we 
     needed a thread in the first  place :) */

  const SampleStruct *sbuf = reader->readAll(); 

  int n_read = reader->numLastRead();

  for (int i = 0; i < n_read; i++) {
    unsigned int channel_id = sbuf[i].channel_id;

    if (graphListenerExists(channel_id)) {
      double unitdata = sdev.sampleToUnits(sbuf[i]);
      listeningGraphs.graphs[channel_id]->graph->plot(unitdata);
    }
  }
  QTimer::singleShot(1, this, SLOT(loop()));
}


/* adds a listening graph to the channel list for channel_id list */
void 
ReaderLoop::
addListeningGraph(DAQECGGraphContainer * g) 
{ 
 
  const unsigned int chan = g->channelId();
  if (chan >= listeningGraphs.n_graphs || listeningGraphs.graphs[chan]) 
    return;

  listeningGraphs.graphs[chan] = g;

}

void
ReaderLoop::
removeListeningGraph(unsigned int chan) 
{
  if (chan >= listeningGraphs.n_graphs)
    return;

  if (listeningGraphs.graphs[chan]) {
    listeningGraphs.graphs[chan] = 0;
  }
}
