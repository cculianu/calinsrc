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
#include <qhbuttongroup.h>
#include <qvbuttongroup.h>
#include <qtoolbar.h>
#include <qlistbox.h>
#include <qpushbutton.h>
#include <qtoolbutton.h>
#include <qtooltip.h>
#include <qinputdialog.h> 
#include <qmessagebox.h>
#include <qtimer.h>
#include <qfile.h>
#include <qdir.h>
#include <qstringlist.h>
#include <qtextedit.h>
#include <qhbox.h>
#include <qvbox.h>
#include <qhgroupbox.h>
#include <qvgroupbox.h>
#include <qkeysequence.h>
#include <qpainter.h>
#include <qprinter.h>
#include <qpaintdevicemetrics.h>
#include <qfont.h>
#include <qfontmetrics.h>
#include <qpixmap.h>
#include <qtextbrowser.h>
#include <qwhatsthis.h>

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
#include "shm.h"
#include "shared_stuff.h"
#include "ecggraph.h"
#include "ecggraphcontainer.h"
#include "daq_system.h"
#include "common.h"
#include "daq_mime_sources.h"
#include "daq_help_browser.h"
#include "simple_text_editor.h"
#include "plugin.h"
#include "comedi_coprocess.h"
#include "daq_images.h"

/* 
   An Opaque Class that handles the log window.. this allows us
   to add that custom CTRL+T accelerator for pasting timestamps into the 
   log ... 
   This is a VERY cheap hack!!
*/
#include <qaccel.h> /* needed in this class... */
class ExperimentLog : public SimpleTextEditor
{
public:
  ExperimentLog(DAQSystem & d, QWidget * parent = 0, const char * name = 0) 
    : SimpleTextEditor(parent, name), daqSystem(d) { init(); }
    
  ExperimentLog(DAQSystem & d, const QString & file, 
                const QString & log_template = QString::null, 
                QWidget * parent = 0, 
                const char * name = 0)
    : SimpleTextEditor (file, log_template, parent, name), daqSystem (d)  
  { init(); }
  
private:
  QAccel * accel;
  void init();
  DAQSystem & daqSystem;
};

void ExperimentLog::init()
{
  setIcon(QPixmap(DAQImages::log_img));
  accel = new QAccel(this); /* to be auto-deleted since it's a child of ours */
  int id = accel->insertItem(CTRL + Key_T);
  accel->connectItem( id, &daqSystem, SLOT(logTimeStamp()) );
}

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
  shmCtl(*readerLoop.shmCtl),
  log(0),
  tyler(&ws),
  plugin_menu(this, 0, QString(name) + " - Plugin Menu"),
  windowTemplateDlg(0)
{
  setIcon(QPixmap(DAQImages::daq_system_img));

  log = new ExperimentLog(*this, 0, "Log Window");


  printer.setPageSize(settings.getPageSize());

  this->setCaption(QString("%1 - %2").arg(name).arg(settings.getDataFile()));
  this->setCentralWidget(&ws);
  ws.setScrollBarsEnabled(true);
  ws.show();

  plugin_menu.setCaption(plugin_menu.name());
  
  statusBar.addWidget(&statusBarScanIndex);

  configWindow.startupScreenSemantics = false;

  { /* build menus */
    fileMenu.insertItem(QIconSet(QPixmap(DAQImages::plugins_img)),
                        "&Plugins...", &plugin_menu, SLOT(raisenShow() ) );
    fileMenu.insertSeparator(); /* ---- */
    fileMenu.insertItem(QIconSet(QPixmap(DAQImages::print_img)),
                        "P&rint...", this, SLOT(printDialog()));
    fileMenu.insertItem(QIconSet(QPixmap(DAQImages::configuration_img)),
                        "&Options", &configWindow, SLOT ( show() ) );
    fileMenu.insertSeparator(); /* ---- */
    fileMenu.insertItem(QIconSet(QPixmap(DAQImages::quit_img)),
                        "&Quit", this, SLOT( close() ) );

    logMenu.insertItem(QIconSet(QPixmap(DAQImages::log_img)),
                       "Show &Log Window", this, SLOT (showLogWindow()), 
                       CTRL + Key_L);
    logMenu.insertSeparator(); /* ---- */
    logMenu.insertItem(QIconSet(QPixmap(DAQImages::timestamp_img)),
                       "Insert &Timestamp", this, SLOT (logTimeStamp()), 
                       CTRL + Key_T);
    channelsMenu.insertItem(QIconSet(QPixmap(DAQImages::add_channel_img)), 
                            "&Add Channel...", this, SLOT( addChannel() ), 
                            CTRL + Key_A );
    channelsMenu.insertItem(QIconSet(QPixmap(DAQImages::synch_img)),
                            "&Synchronize Channels", this, SLOT(resynch()));

    channelsMenu.insertItem("Set Analog Input &Reference Mode...", this, SLOT( changeAREFDialog() ), CTRL + Key_R );
    
    windowMenu.insertItem(QIconSet(QPixmap(DAQImages::wintemplates_img)),
                          "&Window Templates...", 
                          this, 
                          SLOT ( showWindowTemplateDialog() ));
    windowMenu.insertSeparator(); /* ---- */
    windowMenu.insertItem("&Cascade Channel Windows", &ws, SLOT( cascade() ) );
    windowMenu.insertItem("&Tile Channel Windows", &tyler, SLOT( tyle() ) );
    windowMenu.insertSeparator(); /* after this, all open windows will be
                                     stored */
    connect(&windowMenu, SIGNAL(activated(int)), 
	    this, SLOT(windowMenuFocusWindow(int)));

    helpMenu.insertItem("DAQ System Help", help1);
    helpMenu.insertItem("Configuring DAQ System", help2);
    helpMenu.insertItem("&About", help3);

    connect(&helpMenu, SIGNAL(activated(int)), 
            this, SLOT(daqHelp(int)));

    _menuBar.insertItem("&File", &fileMenu);
    _menuBar.insertItem("&Log", &logMenu);
    _menuBar.insertItem("&Channels", &channelsMenu);
    _menuBar.insertItem("&Window", &windowMenu);
    _menuBar.insertSeparator(); /* ---- */
    _menuBar.insertItem("&Help", &helpMenu);
  }


  { /* log window stuff */    

    log->loadTemplate(settings.getTemplateFileName()); // load the template

    /* set the default log savefile name -- which is datafile, minus 
       extension, plus .log */
    QString logfile 
      = forceFilenameExt(settings.getDataFile(), QString(".log"));   

    log->setOutFile(logfile);
    windowMenuAddWindow(log); /* add this window to the windowMenu 
                                 QPopupMenu */
  }


  { /* add toolbar */

    addChannelB.setPixmap( DAQImages::add_channel_img );
    QToolTip::add( &addChannelB, "Add Channel..." );
    connect(&addChannelB, SIGNAL(clicked()), this, SLOT (addChannel()));
    
    resynchB.setPixmap( DAQImages::synch_img );
    QToolTip::add( &resynchB, "Synchronize Channels" );
    connect(&resynchB, SIGNAL(clicked()), this, SLOT (resynch()));

    timeStampB.setPixmap( DAQImages::timestamp_img );
    QToolTip::add( &timeStampB, "Insert Timestamp into Log" );
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
  uint n_chans_this_device = configWindow.selectedDevice().find().n_channels;
  for (set<uint>::iterator i = s.begin(); i != s.end();  i++) 
    if (*i < n_chans_this_device) /* ignore non-existant channels in
                                     case we just changed boards */
      openChannelWindow(*i, 0, 10); /* modify this to read other defaults too*/
    
  /* KLUDGE -- due to implementation quirks in QWorkspace, a resynch() must
     be called on a non-hidden DAQSystem.. */
  show();
  resynch(); /* so that open windows in DAQSystem (if any) 
                get tiled/synchronized */
  hide();

  /* now begin the main loop that grabs data and plots it */
  readerLoop.loop();

}

DAQSystem::~DAQSystem()
{
  settings.saveSettings(); /* commit settings (usually just window setting
                              changes) */
  if (log)  delete log; 
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
  if (ok)  openChannelWindow(chan, range, n_secs);
  
}

void DAQSystem::changeAREFDialog()
{
  /* slot here */
  
  /* assume first channel represents the AREF setting for all channels...
     since DAQ System enforces 1 AREF for all the channels */
  ComediChannel::AnalogRef current = 
    ComediChannel::int2ar(shmCtl.channelAREF(ComediSubDevice::AnalogInput, 0));

  
  QDialog *d = new QDialog(this, "Change Analog Reference Dialog",
                           TRUE, WDestructiveClose);

  d->setCaption("Change Analog Reference Mode");

  QGridLayout *l =  new QGridLayout(d, 1, 1);

  QVBox *vbox = new QVBox(d);

  l->addWidget(vbox, 0, 0);
  
  QVButtonGroup *button_g = 
    new QVButtonGroup("Change the Analog Reference for all the channels to: ",
                      vbox);

  button_g->setRadioButtonExclusive(true);

  button_g->setSizePolicy(QSizePolicy(QSizePolicy::MinimumExpanding, 
                                      QSizePolicy::MinimumExpanding));

  map<ComediChannel::AnalogRef, QRadioButton *> buttons;
  map<ComediChannel::AnalogRef, QRadioButton *>::iterator it;

  buttons[ComediChannel::Ground] = new QRadioButton("AREF Ground", button_g);
  buttons[ComediChannel::Common] = new QRadioButton("AREF Common", button_g);
  buttons[ComediChannel::Differential] = 
    new QRadioButton("AREF Differential", button_g);
  buttons[ComediChannel::Other] =  new QRadioButton("AREF Other", button_g);
  
  
  QWhatsThis::add(buttons[ComediChannel::Ground],
                  "AREF Ground is for non-referenced single-ended inputs.  "
                  "This setting is most appropriate in systems where the "
                  "signal source and the data acquisition board share a "
                  "commmon ground.  The low and high inputs are always "
                  "taken with respect to the DAQ board's ground.");
  
  QWhatsThis::add(buttons[ComediChannel::Common], 
                  "AREF Common is for referenced single-ended inputs. "
                  "This setting is most appropriate when all the low "
                  "wires of all the channels are tied together but different "
                  "from ground.");
  
  QWhatsThis::add(buttons[ComediChannel::Differential], 
                  "AREF Differential is used when you want to take the "
                  "difference of two channels. This setting is most "
                  "appropriate in systems where the signal source and the "
                  "data acquisition board have differences in their "
                  "ground levels.");

  QWhatsThis::add(buttons[ComediChannel::Other],
                  "This is for any other Analog Reference mode which may "
                  "be supported by your board.");
  
  if (buttons[current])
    buttons[current]->setChecked(true);

  QHBox *hbox = new QHBox(vbox);

  QPushButton *ok = new QPushButton("Ok", hbox), 
              *cancel = new QPushButton("Cancel", hbox);
              
  cancel->setAutoDefault(true);

  connect(ok, SIGNAL(clicked()), d, SLOT(accept()));
  connect(cancel, SIGNAL(clicked()), d, SLOT(reject()));

  ok->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
  cancel->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));

  if (d->exec() == QDialog::Accepted) {
    for (it = buttons.begin(); it != buttons.end(); it++) {
      if (it->second->isChecked()) { changeAREF(it->first); break; }
    }
  }
}

void DAQSystem::changeAREF(ComediChannel::AnalogRef aref) 
{
  vector<uint> chanstate;
  
  /* make this semi-transactional */
  allChannelsOffSaveState(chanstate);
  shmCtl.setAREFAll(ComediSubDevice::AnalogInput, ComediChannel::ar2int(aref));
  channelsOn(chanstate);
}

/* used solely for below method */
const QString DAQSystem::helpMenuDestinations[n_help_dests] = 
{
  DAQMimeSources::HTML::index,
  DAQMimeSources::HTML::configWindow,
  DAQMimeSources::HTML::about
};

void DAQSystem::daqHelp( int id )
{
  if ( id >= help1 && id < n_help_dests )
    DAQHelpBrowser::getDefaultBrowser()->openPage( helpMenuDestinations[id] );
  
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
  openDialog.setCaption("Add a Channel");

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
      = currentDevice().find();
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

      /* build the range combo box */
      QString tmpRangeStr = subdev.generateRangeString(i);
      if (!tmpRangeStr.isNull()) gain.insertItem(tmpRangeStr);

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

static void defaultifyChannelParams (ECGGraphContainer *gcont, 
                                     ShmController & shmCtl)
{
  const uint & chan = gcont->channelId;
  ECGGraph *& graph = gcont->graph;

  graph->unsetSpikeThreshold();
  gcont->setSpikeBlanking( static_cast<int>(DEFAULT_SPIKE_BLANKING) );
  gcont->setSpikePolarity( Positive );
  shmCtl.setSpikeEnabled ( chan, false );
  shmCtl.setSpikeBlanking( chan, DEFAULT_SPIKE_BLANKING ); 
  shmCtl.setSpikePolarity( chan, Positive );
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

  gcont->setIcon(QPixmap(DAQImages::channel_img));

  buildRangeSettings(gcont);

  windowMenuAddWindow(gcont); /* add this window to the 
                                 windowMenu QPopupMenu */

  gcont -> show(); /* will this help with window setting/geometry? */
  readerLoop.producers[chan].add(gcont);
  /* see about getting rid of these signal/slots and use natural signalling
     in producer/consumer classes.. -CC */
  connect(gcont, SIGNAL(closing(const ECGGraphContainer *)),
          this, SLOT(removeGraphFromSettings(const ECGGraphContainer *)));
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

  gcont->rangeChange( range ); /* hopefully above slot will take effect 
                                  and update rt_process appropriately? */

  if (! pos.isNull() ) { /* means we had a default setting */
    gcont->resize(pos.size());
    gcont->move(pos.x(), pos.y());
    gcont->show();
  }

  /* set chan params to default values .. */
  defaultifyChannelParams(gcont, shmCtl); /* static function */
  
  if (! chanParams.isNull() ) { /* means we had a default setting */

    if (chanParams.spike_on) {
      graph->setSpikeThreshold(chanParams.spike_thold); 
      /* signal from graph will propagate this information out to the 
         Shared Memory for the rt_process */
    }

    gcont->rangeChange(chanParams.range);
    gcont->setSpikePolarity( chanParams.spike_polarity );
    gcont->setSpikeBlanking( static_cast<int>(chanParams.spike_blanking) );
    shmCtl.setSpikeBlanking( chan, chanParams.spike_blanking );
    shmCtl.setSpikePolarity( chan, chanParams.spike_polarity );

    graph->setSecondsVisible( chanParams.n_secs );
  } 
 
  resynch(); /* synchronize all the graph displays */

  /* ladies and gentlemen, start your samplings!! */
  shmCtl.setChannel(ComediSubDevice::AnalogInput, chan, true);
  emit channelOpened(chan);

}

/* this slot saves the settings for a specific opened graphcontainer,
   gcont.  If gcont is NULL, then it does the above for ALL open
   graphs. */
void
DAQSystem::saveGraphSettings(const ECGGraphContainer  *gcont)
{
  vector <const ECGGraphContainer *> graphs;
  vector <const ECGGraphContainer *>::iterator it;

  if (!gcont) 
    graphs = graphContainers();
  else 
    graphs.push_back(gcont);
  
  for (it = graphs.begin(); it != graphs.end() && (gcont = *it); it++) {
    uint chan = gcont->channelId; 

    /* save actual window settings */
    QRect pos(gcont->pos(), gcont->size());
    settings.setWindowSetting(chan, pos);
    DAQSettings::ChannelParams cp;
    const ECGGraph *graph = gcont->graph;
    cp.n_secs = graph->secondsVisible();
    cp.range = gcont->currentRange();
    cp.spike_on = graph->spikeMode();
    cp.spike_thold = graph->spikeThreshold();
    cp.spike_polarity = shmCtl.spikePolarity(chan);
    cp.spike_blanking = shmCtl.spikeBlanking(chan); 
    cp.setNull(false);
    settings.setChannelParameters(chan, cp);
  }
}

void
DAQSystem::removeGraphFromSettings(const ECGGraphContainer  *gcont)
{
  vector <const ECGGraphContainer *> graphs;
  vector <const ECGGraphContainer *>::iterator it;
 
  if (!gcont) 
    graphs = graphContainers();
  else
    graphs.push_back(gcont);

  for (it = graphs.begin(); it != graphs.end() && (gcont = *it); it++) {

    /* clear window settings to indicte this window should not be auto-opened
       the next time */
    settings.setWindowSetting(gcont->channelId, QRect());
    /* clear channel parameters */
    settings.setChannelParameters(gcont->channelId, 
                                  DAQSettings::ChannelParams());
  }
}


void 
DAQSystem::logTimeStamp()
{
  log->insertAtCursor(statusBarScanIndex.text());
  if (!log->isActiveWindow()) showLogWindow();
}

void 
DAQSystem::closeEvent (QCloseEvent * e)
{
  if (!log->checkUnsaved()) return;

  saveGraphSettings();

  QMainWindow::closeEvent(e);
}

/* this slot does the necessary work to tell the rt-process that a 
   channel's range/gain setting needs to be changed */
void
DAQSystem::
graphChangedRange(uint channel, int range)
{
  shmCtl.setChannelRange(ComediSubDevice::AnalogInput, channel, 
                         (uint)range);
}

void
DAQSystem::setStatusBarScanIndex(scan_index_t index)
{
  statusBarScanIndex.
    setText(QString("Scan Index %1").arg(uint64_to_cstr(index)));
}

void
DAQSystem::windowMenuFocusWindow(int id)
{
  if (menuIdToWindowMap.find(id) != menuIdToWindowMap.end()) 
    windowMenuFocusWindow(menuIdToWindowMap[ id ]);  
}

void DAQSystem::windowMenuFocusWindow(QWidget *w)
{
    w->hide();
    w->show();     /* just in case its hidden */					   
    w->setFocus(); /* is this how we activate it? */  
}

void DAQSystem::spikePolarityChanged(SpikePolarity p) 
{
  const ECGGraphContainer *sender = 
    dynamic_cast<const ECGGraphContainer *>(this->sender());
  
  if ( sender )
    shmCtl.setSpikePolarity(sender->channelId, p);
}

void DAQSystem::spikeBlankingChanged(uint b) 
{
  const ECGGraphContainer *sender = 
    dynamic_cast<const ECGGraphContainer *>(this->sender());

  if ( sender )
    shmCtl.setSpikeBlanking(sender->channelId, b);
}

void DAQSystem::spikeThresholdSet(double value) 
{
  const QWidget *w = dynamic_cast<const QWidget *>(this->sender());
  const ECGGraphContainer *gc
    = ( w 
        ? dynamic_cast<const ECGGraphContainer *>(w->parentWidget()) 
        : 0 );
  
  if ( gc ) {
    shmCtl.setSpikeThreshold(gc->channelId, value);
    shmCtl.setSpikeEnabled(gc->channelId, true);
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
    shmCtl.setSpikeEnabled(gc->channelId, false);
}

int /* returns the window id */
DAQSystem::windowMenuAddWindow(QWidget *w)
{
  int ret = ( 
    
               w->icon() 
               ?   windowMenu.insertItem(QIconSet(*w->icon()), w->name())
               :   windowMenu.insertItem(w->name()) 

            );

  
  /* now add this window to our 'Window' QPopupMenu */
  menuIdToWindowMap [ ret ] = w;
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

/* defeat qt's type-dumbness with signals/slots */
void 
DAQSystem::windowMenuRemoveWindow(const ECGGraphContainer *w)
{ windowMenuRemoveWindow(static_cast<const QWidget *>(w)); }

void
DAQSystem::graphOff(const ECGGraphContainer * gcont)
{
  shmCtl.setChannel(ComediSubDevice::AnalogInput, gcont->channelId, false);
  /* this is necessary so as to inform sample writer in some cases.. */
  readerLoop.turnOffChannel(gcont->channelId); 
  emit channelClosed(gcont->channelId);
}

void DAQSystem::closeGraphWindow( int channel_id )
{
  vector<const ECGGraphContainer *>gcs = graphContainers();
  vector<const ECGGraphContainer *>::iterator it;

  for (it = gcs.begin(); it != gcs.end(); it++) {
    if ((*it)->channelId == static_cast<uint>(channel_id)) 
      const_cast<ECGGraphContainer *>(*it)->close(true);
  }
}

void DAQSystem::closeAllGraphWindows()
{
  vector<const ECGGraphContainer *>gcs = graphContainers();
  vector<const ECGGraphContainer *>::iterator it;

  for (it = gcs.begin(); it != gcs.end(); it++) 
    closeGraphWindow((*it)->channelId);
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
    currentDevice().find().ranges();

  for (uint i = 0; i < ranges.size(); i++) {
    c->addRangeSetting(ranges[i].min, ranges[i].max,
                       ( ranges[i].unit == UNIT_volt 
                         ? ECGGraphContainer::Volts
                         : ECGGraphContainer::MilliVolts ));
  }

}

void DAQSystem::showLogWindow()
{
  windowMenuFocusWindow(log);
}


vector<const ECGGraphContainer *> DAQSystem::graphContainers() const
{
  const ECGGraphContainer *gc;
  vector<const ECGGraphContainer *> ret;
  map <int, QWidget *>::const_iterator it = menuIdToWindowMap.begin();
  map <uint, const ECGGraphContainer *> m;
  map <uint, const ECGGraphContainer *>::const_iterator m_it;

  for(; it != menuIdToWindowMap.end(); it++) 
    if ( (gc = dynamic_cast<const ECGGraphContainer *>(it->second)) )
      m [ gc->channelId ] = gc; // sorts them my putting them in a map
  
  for (m_it = m.begin(); m_it != m.end(); m_it++)
    ret.push_back(m_it->second);
  
  return ret;
}

void DAQSystem::printDialog()
{
  QDialog whichGraphs(this, 0, true, WStyle_Customize|WStyle_NormalBorder
                                     |WStyle_Title|WStyle_SysMenu);
  whichGraphs.setCaption("Select Graphs to Print");
  QGridLayout l(&whichGraphs, 1, 1);
  QVBox *vbox = new QVBox(&whichGraphs);
  l.addWidget(vbox, 0, 0);
  QHGroupBox *hgbox = new QHGroupBox("Select graphs to print:", vbox);
  
  vector<const ECGGraphContainer *> gcs(graphContainers()), paused;
  map<const ECGGraphContainer *, QCheckBox *> checkboxen;

  for (uint i = 0; i < gcs.size(); i++) 
    checkboxen [ gcs[i] ] = new QCheckBox(gcs[i]->name(), hgbox);
  
  QHBox *hbox = new QHBox(vbox);
  QPushButton *b = new QPushButton("Print", hbox);
  connect(b, SIGNAL(pressed()), &whichGraphs, SLOT(accept()));
  b->setAccel(QKeySequence(Key_Enter));
  b = new QPushButton("Cancel", hbox);
  b->setAccel(QKeySequence(Key_Escape));
  connect(b, SIGNAL(pressed()), &whichGraphs, SLOT(reject()));

  /* pause everything to be all dramatic... */
  for (uint i = 0; i < gcs.size(); i++) 
    if (!gcs[i]->isPaused()) {
      const_cast<ECGGraphContainer *>(gcs[i])->pause();
      paused.push_back(gcs[i]);
    }

  int result = whichGraphs.exec();

  
  if (result == QDialog::Accepted) {    
    /* harvest selected boxes */
    vector<const ECGGraphContainer *>::iterator i;
    for ( i = gcs.begin(); i != gcs.end(); i++)
      if (!checkboxen[*i]->isChecked()) gcs.erase(i--);
    print(gcs);
  }

  /* now unpause everything that was paused before */
  for (uint i = 0; i < paused.size(); i++) 
    const_cast<ECGGraphContainer *>(paused[i])->pause();

}

void DAQSystem::print(vector<const ECGGraphContainer *> & gcs) 
{
  static const int margin = 10; /* the width of the border around the page, 
                                   in pixels */
  static const double graph_aspect_ratio = 2.0; /* the ratio of width/height
                                                   to which to render the 
                                                   graph */

  if (gcs.size() == 0) return;

  printer.setMinMax(1, gcs.size());
  printer.setFromTo(0, 0);

  if (!printer.setup(0)) return;

  settings.setPageSize(printer.pageSize());

  QPainter painter;
  painter.begin(&printer);

  QPaintDeviceMetrics pmetrics(&printer);

  int next_page, i, start_page, endloop_page, numcopies = printer.numCopies();

  switch (printer.pageOrder()) {
  case QPrinter::LastPageFirst:
    start_page = printer.toPage() - 1;
    next_page = -1;
    endloop_page = printer.fromPage() - 2;
    break;
  default:
  case QPrinter::FirstPageFirst:
    start_page = printer.fromPage() - 1;
    next_page = 1;
    endloop_page = printer.toPage();
    break;
  }

  while (numcopies-- > 0)
    for (i = start_page; i != endloop_page; /*inside loop*/) {
      QFont font;
      font.setPointSize(18);

      painter.setFont(font);
  
      QFontMetrics fmetrics(painter.fontMetrics());

      QPoint topleft(margin,margin);
    
      painter.drawText(topleft, QString(gcs[i]->name()) + " (" + gcs[i]->currentRangeText() + ")");
      topleft += QPoint(0, margin + fmetrics.height());

      int pm_width = pmetrics.width() - 2 * margin,
          pm_height = static_cast<int>(pm_width / graph_aspect_ratio);

      font.setPointSize(10);
      painter.setFont(font);
      fmetrics = painter.fontMetrics();

      /* make sure we have room for the axis labels... */
      if (pm_height + topleft.y() + 2*margin + fmetrics.height() > pmetrics.height())  
        pm_height -= 2*margin + fmetrics.height();
      

      QPixmap pm(pm_width, pm_height);
      /* FIXME: see why in greyscale pixmap ends up inverted? */

      gcs[i]->graph->renderToPixmap(pm);
      painter.drawPixmap(topleft.x(), topleft.y(), pm);
      
      /* draw x-axis labels.. */
      topleft += QPoint(0, pm_height + margin);
      vector<QString> xaxis_text = gcs[i]->xAxisText();
      for (uint j = 0; j < xaxis_text.size(); j++ ) {
        painter.drawText(topleft, xaxis_text[j]);
        topleft += QPoint(pm_width / xaxis_text.size(), 0);
      }
      
      if ( (i += next_page) != endloop_page || numcopies) printer.newPage();
    }
  painter.end();
}

void DAQSystem::allChannelsOffSaveState (vector<uint> & v)
{
  uint i;

  v.clear();
  
  for (i = 0; i < shmCtl.numChannels(ComediSubDevice::AnalogInput); i++) {
    if (shmCtl.isChanOn(ComediSubDevice::AnalogInput, i)) 
      v.push_back(i);
    shmCtl.setChannel(ComediSubDevice::AnalogInput, i, false);
  }
}

void DAQSystem::channelsOn (const vector<uint> & chanspec)
{
  uint i;

  for (i = 0; i < chanspec.size(); i++) 
    shmCtl.setChannel(ComediSubDevice::AnalogInput, chanspec[i], true);  
}

ReaderLoop::
/* very comedi-specific constructor! Must change! */
ReaderLoop(DAQSystem *d) :
  daq_system(d), 
  pleaseStop(false), 
  n_channels(d->currentdevice.find().n_channels),
  producers(n_channels),
  saved_curr_index(0),
  last_sleep_time(1000)
{
  switch (d->settings.getInputSource()) {
  
  case DAQSettings::RTProcess:
    shmCtl = new ShmController(); /* auto-probe a shm_type */

    source = new SampleStructFIFOSource(string("/dev/rtf") + shmCtl->aiFifoMinor());
    source->flush();

    /* build a non-blocking sample reader */
    reader = new SampleStructReader(source, 0);

    /* build the sample writer */
    switch(d->settings.getDataFileFormat()) {
    case DAQSettings::Binary:
      writer = new SampleBinWriter(shmCtl->samplingRateHz(), 
                                   d->settings.getDataFile().latin1());
      break;
    case DAQSettings::Ascii:
      writer = new SampleGZWriter(shmCtl->samplingRateHz(), 
                                  d->settings.getDataFile().latin1());
      break;
    default:
      throw UnimplementedException("INTERNAL ERROR", 
                                   "Unknown data file format specified in "
                                   "settings");
      break;
    }

    // add the writer as a consumer for all channels
    for (uint i = 0; i < n_channels; i++) producers[i].add(writer);

    break;
  default:
    throw UnimplementedException 
      ("Unimplemented feature",
       "The use of non-rt_process input sources is not yet implemented!\n"
       "Either insmod rtlab.o or give up for now... (Sorry!)");
    break;
  }
}


void DAQSystem::showWindowTemplateDialog()
{
  if (!windowTemplateDlg) {
    windowTemplateDlg = new WindowTemplateDialog(this, "Window Templates", 
                                                 WType_TopLevel);
    windowTemplateDlg->setCaption(windowTemplateDlg->name());
  }
  
  windowTemplateDlg->show();
  windowTemplateDlg->setActiveWindow();
  windowTemplateDlg->raise();
}


ReaderLoop::~ReaderLoop()
{
  pleaseStop = true;
  cerr << (string("Read: ") + reader->numRead()) 
       << (string(" samples without errors, dropped ") + reader->numDropped())
       << " samples." << endl 
       << (reader->numDropped()
           ? "(Dropped samples can occur when the GUI task is too slow"
           " for the\nReal-Time task, or when channels are turned off"
           " then back on, so\n that samples are skipped.)\n"
           : "");
  delete reader; reader = 0;
  delete source; source = 0;
  delete writer; writer = 0;
  delete shmCtl; shmCtl = 0;
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

    if ( sbuf[i].magic_number != SAMPLE_STRUCT_MAGIC ) {
      throw  SerializationException ( "Sample struct magic check failed",
                                      "The sample struct magic number is invalid! Argh!");
    }

    if ( (chan = sbuf[i].channel_id) < producers.size() ) {
      producers[chan].produce(sbuf + i);
    }
  }
    
  { /* emit scan index update every 1 second */
    if (shmCtl->scanIndex() - saved_curr_index > shmCtl->samplingRateHz()) {
      saved_curr_index = shmCtl->scanIndex();
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
   This static function is a very big hack to deal with the 
   inconsistencies/privateness of QWorkspace.  Basically, it was observed 
   through empirical evidence and a little bit of sourcecode perusal that 
   the only way to reliably move a QWidget inside of a QWorkspace is either

      a. to call QWidget::move() on the widget before it is first show()n 
         inside the QWorkspace
      b. emulate (a.) by reparenting the widget out of the QWorkspace then 
         putting it back again.

   As such, the below static function is ultra private, ultra hackish, and 
   may break with future implementations of QWorkspace.  It also has a 
   devilishly hackish way of determining the framesize of a QWidget inside a 
   QWorkspace (basically because of the fact that QWidget::pos() returns the 
   height of the title bar and the width of the left-side frame), which may 
   also break.  But this was the only way to write a window tiling algorithm 
   without having to re-implement QWorkspace.
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
    ECGGraphContainer * graph;
    map<uint, ECGGraphContainer *> graphs; /* sorted by channelId- */
    map<uint, ECGGraphContainer *>::iterator map_it;

    if (!widgets.count()) return;
    uint i, cum_height = 0, width = ws->width(), height = ws->height()/widgets.count();

    /* separate the wheat from the chaff */
    for (i = 0; i < widgets.count(); i++)
      if ( (graph = dynamic_cast<ECGGraphContainer *>(w = widgets.at(i))) == 0)
        nonGraphs.append(w);
      else
        graphs [ graph->channelId ] = graph; /* maps always are sorted by
                                                key, so this ensures we
                                                tile in channel-id order */
    
    /* tile the sorted map */
    for(map_it = graphs.begin(); map_it != graphs.end(); map_it++)
        // map_it->second, ws, and rows get modified
        widget_in_workspace_resize_and_move(map_it->second, 
                                            ws, width, height, cum_height); 

    /* now tack on the non-graphs to the end */
    for (i = 0; i < nonGraphs.count(); i++)
      widget_in_workspace_resize_and_move(nonGraphs.at(i), 
                                          ws, width, height, cum_height);
  }
}

PluginMenu::PluginMenu(DAQSystem * ds, 
                       QWidget *parent, const char *name, WFlags f)
  : QWidget (parent, name, f),
    daqSystem(ds)

{
  setIcon(QPixmap(DAQImages::plugins_img));

  plugin_cmenu = new QPopupMenu(this, QString(name) + " - Plugin Menu Context");
  plugin_cmenu->insertItem("Show Window", this, SLOT(showSelectedWindow()));
  plugin_cmenu->insertItem("Load", this, SLOT(carefullyLoadSelected()));
  plugin_cmenu->insertItem("Unload", this, SLOT(removeSelectedPlugin()));
  plugin_cmenu->insertSeparator();
  plugin_cmenu->insertItem("Unload All", this, SLOT(unloadAll()));
  
  QGridLayout *layout = new QGridLayout(this, 4, 2);
  QLabel *label = new QLabel("All Scanned Plugins:", this);
  plugin_box = new QListView(this, "Plugin Box");
  plugin_box->addColumn("Plugin Name");
  plugin_box->addColumn("Is Loaded?");
  plugin_box->addColumn("Shared object");

  details = new QTextEdit(this);
  details->setTextFormat(Qt::RichText);
  details->setReadOnly(true);

  QPushButton *load_button = new QPushButton("Load Selected", this);    
  QPushButton *remove_button = new QPushButton("Unload selected", this);

  layout->addMultiCellWidget(label, 0, 0, 0, 1);
  layout->addMultiCellWidget(plugin_box, 1, 1, 0, 1);
  layout->setRowStretch(1, 1);
  layout->addMultiCellWidget(details, 2, 2, 0, 1);
  layout->addWidget(load_button, 3, 0);
  layout->addWidget(remove_button, 3, 1);

  
  vector<QString> plugin_search_path;

  { 
    /* build plugin search path */    
    plugin_search_path.push_back(QDir::currentDirPath());
#ifdef DAQ_PLUGINS_PREFIX
    if (QDir(DAQ_PLUGINS_PREFIX).isReadable())
      plugin_search_path.push_back(DAQ_PLUGINS_PREFIX);
#endif
    QString user_plugins_dir = DAQ_SYSTEM_USER_DIR + "/plugins";
    struct stat statbuf;
    if (!stat(user_plugins_dir.latin1(), &statbuf) && S_ISDIR(statbuf.st_mode))
      plugin_search_path.push_back(user_plugins_dir);           
  }

  {
    /* search for plugins and populate the listview */
    vector<QString>::iterator it;

    for (it = plugin_search_path.begin(); it!=plugin_search_path.end(); it++) {
      QDir dir (*it, "*.so", QDir::Name | QDir::IgnoreCase, QDir::Files | QDir::Readable);
      uint i;
      for (i = 0; i < dir.count(); i++) {
        PluginInfo info;
        if (inspectPlugin(*it + "/" + dir[i], info) && 
            findScannedByName(info.name) == scanned_plugins.end() ) { 
          (void)new QListViewItem(plugin_box, info.name, "No", info.short_filename);
          scanned_plugins.push_back(info);          
        }
      }
    }
  }

  if (!scanned_plugins.size()) details->setText("No Plugins were found.");
  

  connect(load_button, SIGNAL(clicked()), this, SLOT(carefullyLoadSelected()));
  connect(remove_button, SIGNAL(clicked()), this, SLOT(removeSelectedPlugin()));
  connect(plugin_box, SIGNAL(doubleClicked(QListViewItem *)), 
          this, SLOT(carefullyLoadSelected()));
  connect(plugin_box, 
          SIGNAL(contextMenuRequested (QListViewItem *, const QPoint &,int)), 
          this, 
          SLOT(pluginMenuContextReq(QListViewItem *, const QPoint &, int)));
  connect(plugin_box, SIGNAL(currentChanged(QListViewItem *)),
          this, SLOT(showDetails(QListViewItem *)));
  connect(plugin_box, SIGNAL(selectionChanged(QListViewItem *)),
          this, SLOT(showDetails(QListViewItem *)));
}

PluginMenu::~PluginMenu()
{
  unloadAll();
}

void PluginMenu::raisenShow()
{
  hide(); 
  show(); 
  raise(); 
  setFocus(); 
}

void PluginMenu::showDetails(QListViewItem *item)
{
  scanned_plugins_it_t sp_it;

  if (!item) goto clear; 

  sp_it = findScannedByName(item->text(0));

  if (sp_it == scanned_plugins.end()) goto clear;

  details->setTextFormat(Qt::RichText);
  details->setText(QString("<h3>Plugin Details</h3><br>"
                           "<b>Plugin Name:</b> %1 (%2)<br>"
                           "<b>Filename:</b> %3<br>"
                           "<b>Description:</b> %4<br>"
                           "<b>Requires:</b> %5<br>"
                           "<b>Author(s):</b> %6</br>")
                   .arg(sp_it->name)
                   .arg(sp_it->short_filename)
                   .arg(sp_it->filename)
                   .arg(sp_it->description)
                   .arg(sp_it->requires)
                   .arg(sp_it->author));
  return;
 clear:
  details->setText(""); 
}

void PluginMenu::carefullyLoadSelected()
{
  QListViewItem *item = plugin_box->currentItem();
  scanned_plugins_it_t sp_it;

  if (!item 
      || (sp_it = findScannedByName(item->text(0))) == scanned_plugins.end() )
    return;
  
  QString plugin_file  = sp_it->filename;
  
  if (plugin_file.isNull() || pluginFindByName(sp_it->name)) 
    return; // already loaded

  try {
    Plugin * plugin;
    void * handle;
    loadPlugin(plugin_file, plugin, handle);
    plugins_and_handles[plugin] = (int *)handle;
    item->setText(1, "Yes");
#ifdef DEBUG
    cerr << "Plugin '" << plugin->name() << "' loaded with address " << reinterpret_cast<void *>(plugin) << endl;
#endif
  } catch (Exception & e) {
    e.showError();   
  }
}

void PluginMenu::showSelectedWindow()
{
  QListViewItem *item = plugin_box->currentItem();

  if (!item) return;

  Plugin *p = pluginFindByName(item->text(0));
  QWidget *w = ( p ? dynamic_cast<QWidget *>(p) : 0);
    
  if (w) {
    daqSystem->windowMenuFocusWindow(w); // just in case it is a child of ds
    w->setFocus(); w->raise(); // in case it is not a child of ds
  }  

}

void PluginMenu::removeSelectedPlugin()
{    
  QListViewItem *item = plugin_box->currentItem();

  if (!item) return;

  Plugin *p = pluginFindByName(item->text(0));  
  if (p)  unloadPlugin(p);     
}

void PluginMenu::pluginMenuContextReq(QListViewItem *item, 
                                      const QPoint & point,
                                      int col)
{  
  Plugin *p = (item ? pluginFindByName(item->text(0)) : 0 );
  QWidget *w = (p ? dynamic_cast<QWidget *>(p) : 0);

  plugin_cmenu->setItemEnabled(plugin_cmenu->idAt(0), /* show window */
                               w && plugin_box->currentItem());
  plugin_cmenu->setItemEnabled(plugin_cmenu->idAt(1), /* load */
                               !p);
  plugin_cmenu->setItemEnabled(plugin_cmenu->idAt(2), /* unload */
                               p);
  plugin_cmenu->setItemEnabled(plugin_cmenu->idAt(4), /* unload all */
                               plugins_and_handles.size());
  plugin_cmenu->popup(point);
  (void)col; // keep compiler happy..
}

void PluginMenu::loadPlugin(const char *filename, Plugin * & plugin, 
                            void * & handle) throw (PluginException)
{
  handle = dlopen(filename, RTLD_NOW | RTLD_GLOBAL);
  plugin = NULL;
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
    throw PluginException ("Cannot find plugin entry point", err); 
  }
  

  try {
    plugin = entry(daqSystem);      
    if (!plugin) { dlclose(handle); return; }
  } catch (PluginException & e) {
    if (plugin) delete plugin;
    dlclose(handle);
    throw;
  }

}

bool PluginMenu::inspectPlugin(const char *filename, PluginInfo & info) const
{
  void *handle = dlopen(filename, RTLD_NOW | RTLD_GLOBAL);
  const char **n, **d, **a, **r;
  const int *v; 
  
  bool ret = false;
  
  if (handle && 
      dlsym(handle, "entry") && 
      (v = (const int *)dlsym(handle, "ds_plugin_ver")) &&
      *v == DS_PLUGIN_VER &&
      (n = (const char **)dlsym(handle, "name"))) {

    info.name = *n;
    
    d = reinterpret_cast<const char **>(dlsym(handle, "description"));
    if (d) info.description = *d; else info.description = QString::null; 
    a = reinterpret_cast<const char **>(dlsym(handle, "author"));
    if (a) info.author = *a; else info.author = QString::null;
    r = reinterpret_cast<const char **>(dlsym(handle, "requires"));
    if (r) info.requires = *r; else info.requires = QString::null;

    info.filename = filename;
    info.short_filename = info.filename;

    info.short_filename = QStringList::split("/", info.short_filename).back();

    ret = true;
  }

  if (handle) dlclose(handle);

  return ret;    
}

PluginMenu::scanned_plugins_it_t 
PluginMenu::findScannedByName(const QString & name) 
{
  scanned_plugins_it_t it;
  
  for (it = scanned_plugins.begin(); it != scanned_plugins.end(); it++) {
    if (it->name == name) break;
  }

  return it;
}

void PluginMenu::unloadPlugin(Plugin *p) 
{
  map<Plugin *, int *>::iterator i;
  QString name(p->name());

  if (p->inUse()) {
    QMessageBox::information(this, "Cannot unload plugin.",
                             QString("Plugin ") + name + " is in use and "
                             "cannot be unloaded.", QMessageBox::Ok);
    return;
  }
  

  for (i = plugins_and_handles.begin(); i != plugins_and_handles.end(); i++) 
    if (i->first == p) {

      delete p;
      dlclose(i->second);
      plugins_and_handles.erase(i--);
#ifdef DEBUG
      cerr << "Plugin '" << name << "' at address " << reinterpret_cast<void *>(p) << " unloaded." << endl;
#endif

    }
  
  QListViewItem *item = plugin_box->findItem(name, 0);
  item->setText(1, "No");
  
}

void PluginMenu::unloadAll()
{ 
  while (plugins_and_handles.begin() != plugins_and_handles.end()) 
    unloadPlugin( plugins_and_handles.begin()->first );  
}

Plugin * PluginMenu::pluginFindByName(QString name) 
{
  map<Plugin *, int *>::iterator i;
  for (i = plugins_and_handles.begin(); i != plugins_and_handles.end(); i++) 
    if (name == i->first->name()) 
      return i->first; /* all plugins call unloadPlugin internally */ 
  return 0;
}


WindowTemplateDialog::WindowTemplateDialog(DAQSystem *parent, 
                                           const char *name = 0, WFlags f = 0)
  : QWidget (parent, name, f)
{
  setIcon(QPixmap(DAQImages::wintemplates_img));
  ds = parent;
  QGridLayout *layout = new QGridLayout(this);
 
  
   
  layout->addWidget((new QLabel("<B>Window Templates</b><p>Window templates allow you to save or load the current window and channel 'state' to/from a named profile.<P>This is useful if you want this program to remember specific channel settings, so they can be recalled at a later date.", new QVGroupBox("Description", this)))->parentWidget(),    0, 0);

  templateNames = new QListBox(new QVGroupBox("Available Templates", this), 
                               "Template Name ListBox");

  QToolTip::add(templateNames, "Right click to create/delete/apply a profile");
  templateNames->setSelectionMode(QListBox::Single);

  connect(templateNames, SIGNAL(selectionChanged()), this, SLOT(autoEnableDisableButtonsAndMenuStuff()));

  connect(templateNames, SIGNAL(selectionChanged()), this, SLOT(updateDetails()));
  layout->addWidget(templateNames->parentWidget(), 1, 0);

  details = new QTextEdit(new QVGroupBox("Details", this));
  details->setReadOnly(true);

  layout->addWidget(details->parentWidget(), 2, 0);

  cmenu = new QPopupMenu(templateNames, "Window Template Context Menu Popup");

  cmenu->insertItem("Create New...", this, SLOT(createNew()));
  cmenu->insertSeparator();
  use_id = cmenu->insertItem("Use/Apply to Experiment", this, 
                             SLOT(useSelected()));
  overwrite_id = cmenu->insertItem("Overwrite from Experiment", this, 
                                   SLOT(overwriteSelected()));
  rename_id = cmenu->insertItem("Rename", this, SLOT(renameSelected()));
  delete_id = cmenu->insertItem("Delete", this, SLOT(deleteSelected()));

  connect (templateNames, SIGNAL(contextMenuRequested ( QListBoxItem *, const QPoint & )), this, SLOT(contextRequest(QListBoxItem *, const QPoint &)));

  
  QHButtonGroup * hbg = new QHButtonGroup ("Operations", this, "Operations");
  layout->addWidget(hbg, 3, 0);

  QPushButton 
    *createBut = new QPushButton("Create New...", hbg, "Create New Button"),
    *closeBut;



  useBut = new QPushButton("Apply", hbg, "Apply Button");
  renameBut = new QPushButton("Rename", hbg, "Rename Button");
  deleteBut = new QPushButton("Delete", hbg, "Delete Button");
  closeBut = new QPushButton("Close", hbg, "Close Button");

  connect(createBut, SIGNAL(clicked()), this, SLOT(createNew()));
  connect(useBut, SIGNAL(clicked()), this, SLOT(useSelected()));
  connect(renameBut, SIGNAL(clicked()), this, SLOT(renameSelected()));
  connect(deleteBut, SIGNAL(clicked()), this, SLOT(deleteSelected()));
  connect(closeBut, SIGNAL(clicked()), this, SLOT(close()));

  refreshContents();
}

void WindowTemplateDialog::refreshContents()
{ 
  QString selectedName = templateNames->currentText(); // remember selected
  vector<QString> profiles = ds->settings.windowSettingProfiles();
  vector<QString>::iterator it;

  templateNames->clear();

  for (it = profiles.begin(); it != profiles.end(); it++) 
    templateNames->insertItem(*it);

  updateDetails();

  QListBoxItem *l;
  if (!selectedName.isNull() && (l=templateNames->findItem(selectedName,
                                                           Qt::ExactMatch))) {
    // re-enable the old selected item
    templateNames->setSelected(l, true);
  } else {
    // figure out if we still HAVE an item, and if not, disable buttons...
    autoEnableDisableButtonsAndMenuStuff();
  }
}

void WindowTemplateDialog::updateDetails()
{
  QString profileName( templateNames->currentText() );
  DAQSettings::WindowSettingProfile profile 
    = ds->settings.getWindowSettingProfile(profileName);
  map<uint, DAQSettings::ChannelParams>::iterator it;
  
  QString detailStr="";

  for (it = profile.channelParams.begin(); it != profile.channelParams.end();
       it++)
    {
      DAQSettings::ChannelParams & cp = it->second;
      
      if (cp.isNull()) continue;

      
      detailStr += 
        QString("<b>Channel ") + QString().setNum(it->first) + "</b><br>\n" 
        + QString("<u>Range:</u> ") 
        + ds->currentdevice.find().generateRangeString(cp.range) + "<br>\n"
        + QString("<u>Seconds Visible:</u> ") + QString().setNum(cp.n_secs) 
        + "<br>\n" 
        + QString("<u>Spike:</u> ") 
        + (cp.spike_on 
           ?  QString().setNum(cp.spike_thold)  + "V "
              + (cp.spike_polarity == Positive 
                 ? "Positive" 
                 : "Negative") + " polarity, " 
           + QString().setNum(cp.spike_blanking) + "ms blanking" 
             
           : QString("Off")) + "<br>\n" 
        + "<P>\n";
      
    }
  details->setText(detailStr);
}

void
WindowTemplateDialog::contextRequest(QListBoxItem * lbi, const QPoint & p)
{
  (void) lbi; // keep compiler happy
  cmenu->popup(p);
}

void WindowTemplateDialog::createNew()
{
  bool ok;
  QString name;
  
 try_again:
    name
      = QInputDialog::getText( "DAQSystem - New Window Profile",
                               "Current experiment channel/window settings "
                               "will be used in creating the profile. <b> </b>"
                               "Please name the profile:",
                               QLineEdit::Normal, QString::null, &ok, this );  
    name = name.stripWhiteSpace();
    if (ok) {
      if (name.isNull() ) goto try_again;
      if (!ds->settings.getWindowSettingProfile(name).isNull()) {
        QMessageBox::critical(this, 
                              QString("Profile named '")  + name + "' exists.",
                              QString("A profile named '") + name + "' exists."
                              "  Please choose another name.", 
                              QMessageBox::Ok, QMessageBox::NoButton, 
                              QMessageBox::NoButton);
        goto try_again;
      }
      
      /* Set current to a  profile */
      ds->saveGraphSettings();
      ds->settings.currentToProfile(name);
      refreshContents();
    }
}

void WindowTemplateDialog::useSelected()
{
  if (QMessageBox::warning(this, "Are you sure?", "Applying the selected profile will close all the current channels and open a new set of channels.  Are you sure you wish to continue?<b> </b>", QMessageBox::Yes, QMessageBox::No) == QMessageBox::No)
    return;

  ds->closeAllGraphWindows();
  ds->settings.clearAllChannelWindowSettings();

  QString profileName( templateNames->currentText() );
  DAQSettings::WindowSettingProfile profile 
    = ds->settings.getWindowSettingProfile(profileName);
  map<uint, DAQSettings::ChannelParams>::iterator it;
  
  for (it = profile.channelParams.begin(); it != profile.channelParams.end();
       it++)
    {
      uint chan = it->first;
      QRect & pos = profile.windowSettings[chan];
      DAQSettings::ChannelParams & cp = it->second;

      ds->settings.setChannelParameters(chan, cp);
      ds->settings.setWindowSetting(it->first, pos);

      ds->openChannelWindow(chan, cp.range, cp.n_secs);
    }
  
}

void WindowTemplateDialog::renameSelected()
{

  bool ok;
  QString name, origName(templateNames->currentText());
  
 try_again:
    name
      = QInputDialog::getText( "DAQSystem - Rename Window Profile",
                               "Renaming the profile, please enter the new "
                               "name for this profile",
                               QLineEdit::Normal, origName, &ok, this );  
    name = name.stripWhiteSpace();
    if (ok) {
      if (name.isNull() ) goto try_again;
      if (name == origName)  return; /* silently ignore same name */
      if (!ds->settings.getWindowSettingProfile(name).isNull()) {
        QMessageBox::critical(this, 
                              QString("Profile named '")  + name + "' exists.",
                              QString("A profile named '") + name + "' exists."
                              "  Please choose another name.", 
                              QMessageBox::Ok, QMessageBox::NoButton, 
                              QMessageBox::NoButton);
        goto try_again;
      }
    }

    ds->settings.renameWindowSettingProfile(name, origName);
    refreshContents();    
}

void WindowTemplateDialog::deleteSelected()
{
  if (QMessageBox::warning(this, "Are you sure?", "Deleting the selected profile will remove it from the profile list.  This operation cannot be undone.  Are you sure you wish to continue?<b> </b>", QMessageBox::Yes, QMessageBox::No) == QMessageBox::No)
    return;

  ds->settings.deleteWindowSettingProfile(templateNames->currentText());
  refreshContents();
}

void WindowTemplateDialog::overwriteSelected()
{
  if (QMessageBox::warning(this, "Are you sure?", "Continuing will apply the current experiment's channel/window settings to this selected profile, thus overwriting its current settings.  Are you sure you wish to continue?<b> </b>", QMessageBox::Yes, QMessageBox::No) == QMessageBox::No)
    return;

  ds->settings.currentToProfile(templateNames->currentText());
  refreshContents();
}

void WindowTemplateDialog::autoEnableDisableButtonsAndMenuStuff()
{
  QListBoxItem *lbi = templateNames->item(templateNames->currentItem());
  bool enabled = 
    lbi && !ds->settings.getWindowSettingProfile(lbi->text()).isNull();

  cmenu->setItemEnabled(use_id, enabled);
  cmenu->setItemEnabled(delete_id, enabled);
  cmenu->setItemEnabled(rename_id, enabled);
  cmenu->setItemEnabled(overwrite_id, enabled);

  deleteBut->setEnabled(enabled);
  useBut->setEnabled(enabled);
  renameBut->setEnabled(enabled);
}
