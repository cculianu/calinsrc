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
#include <qlistbox.h>
#include <qpushbutton.h>
#include <qtoolbutton.h>
#include <qinputdialog.h> 
#include <qmessagebox.h>
#include <qtimer.h>
#include <qfile.h>

#include <iostream>
#include <map>
#include <vector>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dlfcn.h>

#include "configuration.h"
#include "sample_source.h"
#include "sample_reader.h"
#include "sample_writer.h"
#include "shm_mgr.h"
#include "shared_stuff.h"
#include "ecggraph.h"
#include "ecggraphcontainer.h"
#include "daq_system.h"

#include "plugin.h"

DAQSystem::DAQSystem (ConfigurationWindow  & cw, QWidget * parent = 0, 
		      const char * name = DAQ_SYSTEM_APPNAME_CSTRING, 
		      WFlags f = WType_TopLevel)
: QMainWindow(parent, name, f), 
  configWindow(cw),
  settings(configWindow.daqSettings()),
  currentdevice(cw.selectedDevice()),
  ws(this),
  _menuBar(this),
  mainToolBar("DAQ System Toolbar", this, Top),
  addChannelB(&mainToolBar),
  resynchB(&mainToolBar),
  timeStampB(&mainToolBar),
  statusBar(this),
  statusBarScanIndex(&statusBar),
  readerLoop(this),
  daqSystemIsClosingMode(false), /* for now, this becomes true when
                                    daq system is closing */
  log(&ws, "Log Window"),
  tyler(&ws),
  plugin_menu(this, 0, QString(name) + " - Plugin Menu")
{

  this->setCaption(QString("%1 - %2").arg(name).arg(settings.getDataFile()));
  this->setCentralWidget(&ws);
  ws.setScrollBarsEnabled(true);
  ws.show();

  
  statusBar.addWidget(&statusBarScanIndex);

  configWindow.startupScreenSemantics = false;

  { /* build menus */
    fileMenu.insertItem("&Plugins...", &plugin_menu, SLOT(raisenShow() ) );
    fileMenu.insertSeparator();
    fileMenu.insertItem("&Options", &configWindow, SLOT ( show() ) );
    fileMenu.insertSeparator();
    fileMenu.insertItem("&Quit", this, SLOT( close() ) );

    logMenu.insertSeparator();
    logMenu.insertItem("Insert &Timestamp", this, SLOT (logTimeStamp()), CTRL + Key_T);
    channelsMenu.insertItem("&Add Channel...", this, SLOT( addChannel() ), CTRL + Key_A );
    windowMenu.insertItem("&Cascade", &ws, SLOT( cascade() ) );
    windowMenu.insertItem("&Tile", &tyler, SLOT( tyle() ) );
    windowMenu.insertSeparator(); /* after this, all open windows will be
                                     stored */
    windowMenu.insertItem("&Resynch Channel Windows", this, SLOT (resynch()));
    windowMenu.insertSeparator();

    connect(&windowMenu, SIGNAL(activated(int)), 
	    this, SLOT(windowMenuFocusWindow(int)));

    helpMenu.insertItem("&About", this, SLOT( about() ) );

    _menuBar.insertItem("&File", &fileMenu);
    _menuBar.insertItem("&Log", &logMenu);
    _menuBar.insertItem("&Channels", &channelsMenu);
    _menuBar.insertItem("&Window", &windowMenu);
    _menuBar.insertSeparator();
    _menuBar.insertItem("&Help", &helpMenu);
  }

  /* add the log window */
  log.loadTemplate(settings.getTemplateFileName());
  windowMenuAddWindow(&log); /* add this window to the windowMenu QPopupMenu */


  { /* add toolbar */
    addChannelB.setText("Add Channel...");
    connect(&addChannelB, SIGNAL(clicked()), this, SLOT (addChannel()));
    
    resynchB.setText("Synchronize Channels");
    connect(&resynchB, SIGNAL(clicked()), this, SLOT (resynch()));

    timeStampB.setText("Insert Timestamp into Log");
    connect(&timeStampB, SIGNAL(clicked()), this, SLOT (logTimeStamp()));

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

bool
DAQSystem::isValidDAQSettings(const DAQSettings & s)
{
  QFile of (s.getDataFile());

  
  if (of.exists()) {
    struct stat buf;

    stat(of.name(), &buf);

    if (S_ISDIR(buf.st_mode) ) {

      /* if it's a directory, complain and reject */
      QMessageBox::critical(0,
                            QString("%1 is a directory.").arg(of.name()),
                            QString("%1 is a directory. Specify a "
                                    "non-directory file for "
                                    "output.").arg(of.name()),
                            QMessageBox::Ok, QMessageBox::NoButton);
      return false;

    } else if (S_ISREG(buf.st_mode)) {

      /* don't allow pre-existing regular files */
      return 
        ( 0 ==
          
          QMessageBox::warning 
          (
           0, 
           QString("File %1 exists").arg(of.name()),
           QString("The output filename you have chosen: %1 already exists.\n"
                   "If you choose to continue, this file will be overwritten "
                   "and your data will be lost!").arg(of.name()),
           "Overwrite", 
           "Cancel", 
           QString::null,
           1, 
           1
          )
          
       );
    }
  }
  return true;
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
          && ! readerLoop.producers[i].findByType((ECGGraphContainer *)0)) {
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

static void defaultifyChannelParams (ECGGraphContainer *gcont)
{
  const uint & chan = gcont->channelId;
  ECGGraph *& graph = gcont->graph;

  graph->unsetSpikeThreshold();
  gcont->setSpikeBlanking( static_cast<int>(DEFAULT_SPIKE_BLANKING) );
  gcont->setSpikePolarity( Positive );
  ShmMgr::setSpikeEnabled ( chan, false );
  ShmMgr::setSpikeBlanking( chan, DEFAULT_SPIKE_BLANKING ); 
  ShmMgr::setSpikePolarity( chan, Positive );
}

void
DAQSystem::openChannelWindow(uint chan, uint range, 
                             uint n_secs)
{
  
  QRect pos(settings.getWindowSetting(chan)); /* grab window pos from file */
  
  DAQSettings::ChannelParams chanParams(settings.getChannelParameters(chan));
  
  /* leaky memory prevented by spaghetti-like destructive close
     on the ECGGraphContainer and a signal-emitting
     destructor in DAQECGGraph */
  ECGGraph *graph = new ECGGraph(1000, n_secs);

  /* todo: put the below in a deleteable place! */
  ECGGraphContainer *gcont =
    new ECGGraphContainer(graph, chan, &ws, 
                             QString("Channel %1").arg(chan).latin1());

  buildRangeSettings(gcont);

  windowMenuAddWindow(gcont); /* add this window to the 
                                 windowMenu QPopupMenu */

  gcont -> show(); /* will this help with window setting/geometry? */
  readerLoop.producers[chan].add(gcont);
  /* see about getting rid of these signal/slots and use natural signalling
     in producer/consumer classes.. -CC */
  connect(gcont, SIGNAL(closing(const ECGGraphContainer *)),
          this, SLOT(saveGraphSettings(const ECGGraphContainer *)));
  connect(gcont, SIGNAL(closing(const ECGGraphContainer *)),
          this, SLOT(windowMenuRemoveWindow(const ECGGraphContainer *)));
  connect(gcont, SIGNAL(rangeChanged(uint, int)),
          this, SLOT(graphChangedRange(uint, int)));
  connect(gcont, SIGNAL(closing(const ECGGraphContainer *)),
          this, SLOT(graphOff(const ECGGraphContainer *)));
  /* spike connections */
  connect(gcont, SIGNAL(spikePolarityChanged(SpikePolarity)), 
          this, SLOT(spikePolarityChanged(SpikePolarity)));
  connect(gcont, SIGNAL(spikeBlankingChanged(uint)),
          this, SLOT(spikeBlankingChanged(uint)));
  connect(graph, SIGNAL(spikeThresholdSet(double)),
          this, SLOT(spikeThresholdSet(double)));
  connect(graph, SIGNAL(spikeThresholdUnset(void)),
          this, SLOT(spikeThresholdOff(void)));
  connect(&readerLoop, SIGNAL(spikeDetected(const SampleStruct *)),
          gcont, SLOT(spikeDetected(const SampleStruct *)));

  gcont->rangeChange( range ); /* hopefully above slot will take effect 
                                  and update rt_process appropriately? */

  if (! pos.isNull() ) { /* means we had a default setting */
    gcont->resize(pos.size());
    gcont->move(pos.x(), pos.y());
    gcont->show();
  }

  /* set chan params to default values .. */
  defaultifyChannelParams(gcont); /* static function */
  
  if (! chanParams.isNull() ) { /* means we had a default setting */

    if (chanParams.spike_on) {
      graph->setSpikeThreshold(chanParams.spike_thold); 
      /* signal from graph will propagate this information out to the 
         Shared Memory for the rt_process */
    }

    gcont->rangeChange(chanParams.range);
    gcont->setSpikePolarity( (chanParams.spike_polarity 
                              ? Positive 
                              : Negative) );
    gcont->setSpikeBlanking( static_cast<int>(chanParams.spike_blanking) );
    ShmMgr::setSpikeBlanking( chan, chanParams.spike_blanking );
    ShmMgr::setSpikePolarity( chan, (chanParams.spike_polarity 
                                     ? Positive 
                                     : Negative));

    graph->setSecondsVisible( chanParams.n_secs );
  } 
 
  resynch(); /* synchronize all the graph displays */

  /* ladies and gentlemen, start your samplings!! */
  ShmMgr::setChannel(ComediSubDevice::AnalogInput, chan, true);

}

/* this slot can be called from either the closeEvent of
   the ECGGraphContainers, or from the closeEvent of
   the DAQSystem classes. It behaves slightly differently
   depending on who calls it. (daqSystemIsClosingMode is different). 
   Purpose is to commit graph container window settings to settings file, 
   or flush them from the settings file if they weren't open
   upon application close */
void
DAQSystem::saveGraphSettings(const ECGGraphContainer  *gcont)
{
  if (gcont == 0) return;

  uint chan = gcont->channelId; 

  if (daqSystemIsClosingMode) {
    /* save actual window settings */
    QRect pos(gcont->pos(), gcont->size());
    settings.setWindowSetting(chan, pos);
    DAQSettings::ChannelParams cp;
    const ECGGraph *graph = gcont->graph;
    cp.n_secs = graph->secondsVisible();
    cp.range = gcont->currentRange();
    cp.spike_on = graph->spikeMode();
    cp.spike_thold = graph->spikeThreshold();
    cp.spike_polarity = ShmMgr::spikePolarity(chan);
    cp.spike_blanking = ShmMgr::spikeBlanking(chan); 
    cp.setNull(false);
    settings.setChannelParameters(chan, cp);
  } else {
    /* clear window settings to indicte this window should not be auto-opened
       the next time */
    settings.setWindowSetting(chan, QRect());
    /* clear channel parameters */
    settings.setChannelParameters(chan, DAQSettings::ChannelParams());
  }
}


void 
DAQSystem::logTimeStamp()
{
  log.insertAtCursor(statusBarScanIndex.text());
}

void 
DAQSystem::closeEvent (QCloseEvent * e)
{
  if (!log.checkUnsaved()) return;

  daqSystemIsClosingMode = true; /* this changes the behavior of the
                                    saveGraphSettings() method */
  for (uint i = 0; i < readerLoop.producers.size(); i++) {
    const ECGGraphContainer *gc;
    if ( (gc = readerLoop.producers[i].findByType((ECGGraphContainer *)0)) )
      saveGraphSettings(gc);
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
DAQSystem::setStatusBarScanIndex(scan_index_t index)
{
  statusBarScanIndex.setText(QString("%1").arg((ulong)index));
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

void DAQSystem::spikePolarityChanged(SpikePolarity p) 
{
  const ECGGraphContainer *sender = 
    dynamic_cast<const ECGGraphContainer *>(this->sender());
  
  if ( sender )
    ShmMgr::setSpikePolarity(sender->channelId, p);
}

void DAQSystem::spikeBlankingChanged(uint b) 
{
  const ECGGraphContainer *sender = 
    dynamic_cast<const ECGGraphContainer *>(this->sender());

  if ( sender )
    ShmMgr::setSpikeBlanking(sender->channelId, b);
}

void DAQSystem::spikeThresholdSet(double value) 
{
  const QWidget *w = dynamic_cast<const QWidget *>(this->sender());
  const ECGGraphContainer *gc
    = ( w 
        ? dynamic_cast<const ECGGraphContainer *>(w->parentWidget()) 
        : 0 );
  
  if ( gc ) {
    ShmMgr::setSpikeThreshold(gc->channelId, value);
    ShmMgr::setSpikeEnabled(gc->channelId, true);
  }
}

void DAQSystem::spikeThresholdOff() 
{
  const QWidget *w = dynamic_cast<const QWidget *>(this->sender());
  const ECGGraphContainer *gc
    = ( w 
        ? dynamic_cast<const ECGGraphContainer *>(w->parentWidget()) 
        : 0 );

  if ( gc )
    ShmMgr::setSpikeEnabled(gc->channelId, false);
}

int /* returns the window id */
DAQSystem::windowMenuAddWindow(QWidget *w)
{
  int ret = 0;

  /* now add this window to our 'Window' QPopupMenu */
  menuIdToWindowMap [ (ret = windowMenu.insertItem(w->name())) ] = w;
  /* /add window */
 
  return ret;
}

void
DAQSystem::windowMenuRemoveWindow(int id)
{
  windowMenu.removeItem(id);
  if ( menuIdToWindowMap.find(id) != menuIdToWindowMap.end() )
    menuIdToWindowMap.erase(menuIdToWindowMap.find(id));
}

void
DAQSystem::windowMenuRemoveWindow(const QWidget *w)
{
  map<int, QWidget *>::iterator it;

  for (it = menuIdToWindowMap.begin(); it != menuIdToWindowMap.end(); it++) {
    if (it->second == w) {
      windowMenu.removeItem(it->first);
      menuIdToWindowMap.erase(it);
      return;
    }
  }
}

void
DAQSystem::graphOff(const ECGGraphContainer * gcont)
{
  ShmMgr::setChannel(ComediSubDevice::AnalogInput, gcont->channelId, false);
  /* this is necessary so as to inform sample writer in some cases.. */
  readerLoop.turnOffChannel(gcont->channelId); 
}

void
DAQSystem::resynch() 
{
  map<int, QWidget *>::iterator it;
  ECGGraphContainer *g = 0;

  tyler.tyle();

  for (it = menuIdToWindowMap.begin(); it != menuIdToWindowMap.end(); it++) 
    if ( (g = dynamic_cast<ECGGraphContainer *>(it->second)) ) 
      g->graph->reset();
    
 
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

ReaderLoop::
/* very comedi-specific constructor! Must change! */
ReaderLoop(DAQSystem *d) :
  daq_system(d), 
  pleaseStop(false), 
  n_channels(d->currentdevice.find(ComediSubDevice::AnalogInput).n_channels),
  producers(n_channels),
  saved_curr_index(0),
  last_sleep_time(1000)
{
  switch (d->settings.getInputSource()) {
  case DAQSettings::RTProcess:
    ShmMgr::check();
    ShmMgr::clearSpikeSettings();

    source = new SampleStructRTFSource(ShmMgr::aiFifoMinor());
    source->flush();

    spike_source = new SampleStructRTFSource(ShmMgr::spikeFifoMinor());
    spike_source->flush();

    /* build a non-blocking sample reader */
    reader = new SampleStructReader(source, 0);
    spike_reader = new SampleStructReader(spike_source, 0);

    /* build the sample writer */
    switch(d->settings.getDataFileFormat()) {
    case DAQSettings::Binary:
      writer = new SampleBinWriter(d->settings.getDataFile().latin1());
      break;
    case DAQSettings::Ascii:
      writer = new SampleGZWriter(d->settings.getDataFile().latin1());
      break;
    default:
      throw UnimplementedException("INTERNAL ERROR", "Unknown data file format specified in settings");
      break;
    }
    { /* setup the writer to listen */
      uint *tmp = new uint [n_channels];
      for (uint i = 0; i < n_channels; i++) {
	tmp[i] = i;
      }
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
  delete spike_reader; spike_reader = 0;
  delete spike_source; spike_source = 0;
}

void
ReaderLoop::loop()
{
  
  if (pleaseStop) return;

  /* the following readAll()  may (theoretically) block indefinitely which is 
     why eventually we should think about threading this bad boy... :) */
  
  const SampleStruct *sbuf = reader->readAll(); 

  int n_read = reader->numLastRead(), i;
  uint chan;

  for (i = 0; i < n_read; i++) {    
    if ( (chan = sbuf[i].channel_id) < producers.size() ) {
      producers[chan].produce(sbuf + i);
    }
  }

  /* notify of spikes detected, if any... */
  sbuf = spike_reader->readAll();
  n_read = spike_reader->numLastRead();
  for (i = 0; i < n_read; i++) 
    emit spikeDetected(sbuf + i);
  
  
  { /* emit scan index update every 1 second */
    if (ShmMgr::scanIndex() - saved_curr_index > ShmMgr::samplingRateHz()) {
      saved_curr_index = ShmMgr::scanIndex();
      emit scanIndexChanged(saved_curr_index);
    }
  }

  QTimer::singleShot(last_sleep_time = source->suggestPollWaitTime(), 
                     this, SLOT(loop()));
}

void
ReaderLoop::turnOffChannel(uint chan_id)
{
  QTimer::singleShot((1000 > last_sleep_time ? 1000 : last_sleep_time), 
                     this, SLOT(turnOffPending()));
  pending_off.push_back(chan_id);
}

void
ReaderLoop::turnOffPending()
{
  uint i;

  for (i = 0; i < pending_off.size(); i++)
    writer->channelStateChanged(pending_off[i], false);
  pending_off.clear();

}

/*
   This static function is a very big hack to deal with the inconsistencies/privateness of QWorkspace
   Basically, it was observed through empirical evidence and a little bit of sourcecode perusal that the only
   way to reliably move a QWidget inside of a QWorkspace is either
      a. to call QWidget::move() on the widget before it is first show()n inside the QWorkspace
      b. emulate (a.) by reparenting the widget out of the QWorkspace then putting it back again.
   As such, the below static function is ultra private, ultra hackish, and may break with future implementations
   of QWorkspace.  It also has a devilishly hackish way of determining the framesize of a QWidget inside a QWorkspace
   (basically because of the fact that QWidget::pos() returns the height of the title bar and the width of the left-side
   frame), which may also break.  But this was the only way to write a window tiling algorithm without having to
   re-implement QWorkspace.
*/
static void widget_in_workspace_resize_and_move (QWidget *w, QWorkspace *ws, uint width, uint height, uint & cum_height)
{
    static Qt::WFlags allFlags = ~0;

    /* CHEAP HACK ALERT!! */
    Qt::WFlags origFlags = w->testWFlags(allFlags);
    uint frameVOffset = w->pos().y() + w->pos().x()*2, // even more of a hack to determine frame height
         frameHOffset = w->pos().x() * 2;

    width       -= frameHOffset,
    height      -= frameVOffset;

    if (!w->minimumSizeHint().isNull()) { // this is unreadable, sorry!
      if (height < static_cast<uint>(w->minimumSizeHint().height()))
        height = w->minimumSizeHint().height();
      if (width < static_cast<uint>(w->minimumSizeHint().width()))
        width = w->minimumSizeHint().width();
    }
    w->reparent(0, static_cast<Qt::WFlags>(0), QPoint(0,0) );
    w->resize(width, height); // very very much a hack that will break across windowstyles...
    w->reparent(ws, origFlags, QPoint(0, cum_height));
    cum_height += w->height() + frameVOffset;
    w->show();
}

void
Tyler::tyle()
{
  uint ct;
  /* HACK HACK HACK */
  for (ct = 0; ct < 2; ct++, QApplication::sendPostedEvents ()) { // for some reason this needs to be done twice!! GRRR...
    QWidget * w;
    QWidgetList widgets(ws->windowList()), nonGraphs;
    if (!widgets.count()) return;
    uint i, cum_height = 0, width = ws->width(), height = ws->height()/widgets.count();

    /* separate the wheat from the chaff */
    for (i = 0; i < widgets.count(); i++)
      if ( dynamic_cast<ECGGraphContainer *>(w = widgets.at(i)) == 0)
        nonGraphs.append(w);
      else
        widget_in_workspace_resize_and_move(w, ws, width, height, cum_height); // w, ws, and rows get modified

    /* now tack on the non-graphs to the end */
    for (i = 0; i < nonGraphs.count(); i++)
      widget_in_workspace_resize_and_move(nonGraphs.at(i), ws, width, height, cum_height);
  }
}

PluginMenu::PluginMenu(DAQSystem * ds, 
                       QWidget *parent, const char *name, WFlags f)
  : QWidget (parent, name, f),
    daqSystem(ds)

{
  plugin_cmenu = new QPopupMenu(0, QString(name) + " - Plugin Menu Context");
  plugin_cmenu->insertItem("Unload", this, SLOT(removeSelectedPlugin()));
  plugin_cmenu->insertSeparator();
  plugin_cmenu->insertItem("Unload All", this, SLOT(unloadAll()));
  
  QGridLayout *layout = new QGridLayout(this, 3, 2);
  QLabel *label = new QLabel("Currently Loaded Plugins:", this);
  plugin_box = new QListBox(this, "Plugin Box");
  QPushButton *load_button = new QPushButton("Load a plugin...", this);    
  QPushButton *remove_button = new QPushButton("Unload selected", this);
  layout->addMultiCellWidget(label, 0, 0, 0, 1);
  layout->addMultiCellWidget(plugin_box, 1, 1, 0, 1);
  layout->setRowStretch(1, 1);
  layout->addWidget(load_button, 2, 0);
  layout->addWidget(remove_button, 2, 1);


  connect(load_button, SIGNAL(clicked()), this, SLOT(queryLoadPlugin()));
  connect(remove_button, SIGNAL(clicked()), this, SLOT(removeSelectedPlugin()));
  connect(plugin_box, 
          SIGNAL(contextMenuRequested (QListBoxItem *, const QPoint &)), 
          this, SLOT(pluginMenuContextReq(QListBoxItem *, const QPoint &)));  
}

PluginMenu::~PluginMenu()
{
  unloadAll();
  delete plugin_cmenu;
}

void PluginMenu::raisenShow()
{
  hide(); 
  show(); 
  raise(); 
  setFocus(); 
}

bool PluginMenu::queryLoadPlugin()
{
  bool ret = false;

  QString plugin_file  
    = QFileDialog::getOpenFileName ( QString::null, "Plugins (*.so)", 0, 0, 
                                     "Select a DAQ System plugin to load");

  if (plugin_file.isNull()) return ret;

  try {
    loadPlugin(plugin_file);
    ret = true;
  } catch (Exception & e) {
    e.showError();   
  }
  return ret;
}

void PluginMenu::removeSelectedPlugin()
{    
  QString name(plugin_box->currentText());

  map<Plugin *, int *>::iterator i;
  for (i = plugins_and_handles.begin(); i != plugins_and_handles.end(); i++) 
    if (name == i->first->name()) 
      unloadPlugin(i->first); /* all plugins call unloadPlugin internally */

}

void PluginMenu::pluginMenuContextReq(QListBoxItem *item, const QPoint & point)
{  
  (void)item;
  plugin_cmenu->setItemEnabled(plugin_cmenu->idAt(0), 
                               plugin_box->currentItem() != -1 );
  plugin_cmenu->setItemEnabled(plugin_cmenu->idAt(2),
                               plugin_box->count());
  plugin_cmenu->popup(point);
}

void PluginMenu::loadPlugin(const char *filename) throw (Exception)
{
  void *handle = dlopen(filename, RTLD_NOW);
  Plugin *plugin = NULL;
  plugin_entry_fn_t entry = NULL;
  const char *name = NULL;
  map<Plugin *, int *>::iterator i;

  if (handle == NULL) throw PluginException ("Plugin open failed", dlerror());

  name = (const char *)dlsym(handle, "name");

  /* check for dupes */
  for (i = plugins_and_handles.begin(); i != plugins_and_handles.end(); i++) 
    if (i->second == (int *)handle || QString(name) == i->first->name() ) {
      dlclose(handle);
      throw PluginException("Plugin already loaded.", 
                            "This plugin is already loaded.");
    }
    

  entry = (plugin_entry_fn_t)dlsym(handle, "entry");

  if (entry == NULL) { 
    QString err = dlerror();
    dlclose(handle); 
    throw PluginException ("Cannot find plugin entry", err); 
  }
  

  try {
    plugin = entry(daqSystem);      
    if (!plugin) { dlclose(handle); return; }
  } catch (...) {
    if (plugin) delete plugin;
    dlclose(handle);
    throw;
  }

  plugins_and_handles[plugin] = (int *)handle;
  plugin_box->insertItem(plugin->name());
  
  

#ifdef DEBUG
  cerr << "Plugin '" << plugin->name() << "' loaded with address " << reinterpret_cast<void *>(this) << endl;
#endif
}

void PluginMenu::unloadPlugin(Plugin *p) 
{
  
  map<Plugin *, int *>::iterator i;
  QString name(p->name());

  for (i = plugins_and_handles.begin(); i != plugins_and_handles.end(); i++) 
    if (i->first == p) {
      delete p;
      dlclose(i->second);
      plugins_and_handles.erase(i--);
    }
  
  QListBoxItem *item = plugin_box->findItem(name);
  if ( item )  delete item;
  
}

void PluginMenu::unloadAll()
{ 
  while (plugins_and_handles.begin() != plugins_and_handles.end()) 
    unloadPlugin( plugins_and_handles.begin()->first );  
}
