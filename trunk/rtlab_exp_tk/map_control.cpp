/*
 * This file is part of the RT-Linux Multichannel Data Acquisition System
 *
 * Copyright (C) 2002 David Christini, Calin Culianu
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
#include <qwidget.h>
#include <qlayout.h>
#include <qlabel.h>
#include <qfont.h>
#include <qstring.h>
#include <qgroupbox.h>
#include <qtimer.h>
#include <qcheckbox.h>
#include <qbuttongroup.h>
#include <qradiobutton.h>
#include <qspinbox.h>
#include <qcombobox.h>
#include <qhbox.h>
#include <qvbox.h>
#include <qlineedit.h>
#include <qvalidator.h>
#include <qscrollbar.h>
#include <qmenubar.h>
#include <qpopupmenu.h>
#include <qmessagebox.h>
#include <qfiledialog.h>
#include <qfile.h>
#include <qtextstream.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "common.h"
#include "daq_system.h"
#include "shm.h"
#include "ecggraph.h"
#include "plugin.h"
#include "exception.h"
#include "tempspooler.h"

#include "tweaked_mbuff.h"
#include "map_control.h"

#include "map_control_private.h"
#include "searchable_combo_box.h"

#define RCS_VERSION_STRING  "$Id$"


extern "C" {

  /* Stuff needed by plugin engine... */
  int ds_plugin_ver = DS_PLUGIN_VER;

  const char * name = "Map Control Experiment",
             * description =
              "Some GUI controls for interfacing with the Map Control stimulation "
              "real-time module named 'map_control.o'.  This rtl/rt_process "
              "addon was developed for an experiment in the Cardiac "
              "Electrodynamics Laboratory of  Cornell University.",
             * author =
              "David J. Christini, Ph.D and Calin A. Culianu.",
             * requires =
              "map_control.o be installed into the kernel.  Analog output.";
  
  Plugin * entry(QObject *o) 
  { 

    /* Top-level widget.. parent is root */
    DAQSystem *d = dynamic_cast<DAQSystem *>(o);

    Assert<PluginException>(d, "Map Control Plugin Load Error", 
                            "The Map Control Plugin can only be used in "
                            "conjunction with daq_system!  Sorry!");    
    return new MapControl(d); ;
  } 

};

static const int beats_per_gridline = 100;
static const int num_gridlines = 5;
/* default values for the graphs */
static const double rr_min   = 0.0,
                    rr_max   = 2000.0,
                    stim_min = 0.0,
                    stim_max = 50.0,
                    g_min    = -15.0,
                    g_max    = 15.0;


MapControl::MapControl(DAQSystem *daqSystem_parent) 
  : QObject(daqSystem_parent, ::name), need_to_save(false),  
    shm(NULL), fifo(-1)
{
  daq_shmCtl = &daqSystem_parent->shmController();

  spooler = new TempSpooler<MCSnapShot>("mapcontrol", true);

  determineRTOS(); /* can throw exception here */
  moduleAttach(); /* can throw exception here */

  /* stub.. */
  window = new QWidget (0, name(), Qt::WStyle_Tool);
  window->setCaption(name());

  masterlayout = new QGridLayout(window, 2, 2);

  QMenuBar *mb = new QMenuBar(window);
  masterlayout->addMultiCellWidget(mb, 0, 0, 0, 1);

  QPopupMenu *fileMenu = new QPopupMenu(mb, "File");
  mb->insertItem("&File", fileMenu);
  fileMenu->insertItem("&Save", this, SLOT ( save() ) );
  fileMenu->insertItem("Save&As", this, SLOT ( saveAs() ) );

  /* graphs.. */
  graphs = new QWidget(window);
  masterlayout->addWidget(graphs, 1, 1);
  masterlayout->setColStretch(1, 2);

  graphlayout = new QGridLayout(graphs, 6, 2);
  graphlayout->setColStretch(1, 1);
  
  rr_graph   = new ECGGraph (beats_per_gridline, num_gridlines, 
                             rr_min, rr_max, 
                             graphs, QString(name()) + " - RR Interval");
  stim_graph = new ECGGraph (beats_per_gridline, num_gridlines, 
                             stim_min, stim_max, 
                             graphs, QString(name()) + " - Number of Stimuli");
  
  g_graph    = new ECGGraph (beats_per_gridline, num_gridlines, 
                             g_min, g_max,
                             graphs, QString(name()) + " - G Values");

  g_graph->plotFactor = stim_graph->plotFactor = rr_graph->plotFactor = 2;

  graphlayout->addWidget(rr_graph, 1, 1); graphlayout->setRowStretch(1, 3);
  graphlayout->addWidget(stim_graph, 3, 1); graphlayout->setRowStretch(3, 3);
  graphlayout->addWidget(g_graph, 5, 1); graphlayout->setRowStretch(5, 3);
  

  graphlayout->addWidget(new QLabel("RR Graph", graphs), 0, 1);
  graphlayout->addWidget(new QLabel("Stim Graph", graphs), 2, 1);
  graphlayout->addWidget(new QLabel("G Graph", graphs), 4, 1);

  addAxisLabels();
  /* end graphs... */

  /* controls... */

  controls = new QWidget(window);
  masterlayout->addWidget(controls, 1, 0);

  const int n_ctl_r = 3, n_ctl_c = 1;
  controlslayout = new QGridLayout(controls, n_ctl_r, n_ctl_c);

  { /* stats groupbox */
    QGroupBox *stats = new QGroupBox(1, Vertical, "Stats", controls);
    controlslayout->addMultiCellWidget(stats, 0, 0, 0, n_ctl_c-1);

    QWidget *tmpw = new QWidget(stats);

    QGridLayout *tmplo = new QGridLayout(tmpw, 11, 2);

    tmplo->addWidget(new QLabel("Number of Stims: ", tmpw), 0, 0);
    tmplo->addWidget(new QLabel("Nominal Number of Stims: ", tmpw), 1, 0);
    tmplo->addWidget(new QLabel("RR Interval (ms): ", tmpw), 2, 0);
    tmplo->addWidget(new QLabel("RR Target (ms): ", tmpw), 3, 0);
    tmplo->addWidget(new QLabel("RR Average (ms): ", tmpw), 4, 0);
    tmplo->addWidget(new QLabel("No. of beats in RR Avg.: ", tmpw), 5, 0);
    tmplo->addWidget(new QLabel("Value of g: ", tmpw), 6, 0);
    tmplo->addWidget(new QLabel("g-Too-Small-Count: ", tmpw), 7, 0);
    tmplo->addWidget(new QLabel("g-Too-Big-Count: ", tmpw), 8, 0);
    tmplo->addWidget(new QLabel("Delta g: ", tmpw), 9, 0);
    tmplo->addWidget(new QLabel("Last beat scan index: ", tmpw), 10, 0);
    
    tmplo->addWidget(current_stim = new QLabel(tmpw), 0, 1);
    tmplo->addWidget(current_nom_stim = new QLabel(tmpw), 1, 1);
    tmplo->addWidget(current_rri = new QLabel(tmpw), 2, 1); 
    tmplo->addWidget(current_rrt = new QLabel(tmpw), 3, 1); 
    tmplo->addWidget(current_rr_avg = new QLabel(tmpw), 4, 1);
    tmplo->addWidget(current_num_rr_avg = new QLabel(tmpw), 5, 1);    
    tmplo->addWidget(current_g   = new QLabel(tmpw), 6, 1);
    tmplo->addWidget(current_g2small = new QLabel(tmpw), 7, 1);
    tmplo->addWidget(current_g2big = new QLabel(tmpw), 8, 1);
    tmplo->addWidget(current_delta_g = new QLabel(tmpw), 9, 1);
    tmplo->addWidget(last_beat_index = new QLabel(tmpw), 10, 1);

  }
  
  { /* controls groupbox */
    QGroupBox *ctls = new QGroupBox(1, Vertical, "Controls", controls);
    controlslayout->addMultiCellWidget(ctls, 1, 1, 0, n_ctl_c-1);

    QVBox *tmpvb = new QVBox(ctls);

    QHBox *tmphb = new QHBox(tmpvb);

    (void) new QLabel("AI Channel to Monitor:", tmphb);
    ai_channels = new SearchableComboBox(tmphb);
    connect(ai_channels, SIGNAL(activated(const QString &)), this, SLOT(changeAIChan(const QString &)));
    connect(daqSystem(), SIGNAL(channelOpened(uint)), this, SLOT(synchAIChan()));
    connect(daqSystem(), SIGNAL(channelClosed(uint)), this, SLOT(synchAIChan()));
    synchAIChan();

    (void) new QLabel("  RR Target:", tmphb);
    rr_target = new QSpinBox(static_cast<int>(rr_graph->rangeMin()),
                             static_cast<int>(rr_graph->rangeMax()),
                             1, tmphb);
    rr_target->setValue(static_cast<int>(shm->rr_target));
    connect(rr_target, SIGNAL(valueChanged(int)), this, SLOT(changeRRTarget(int)));
    

    /* the analog output-related controls */
    tmphb = new QHBox(tmpvb);        
    tmphb->setSpacing(6);
    
    
    (void) new QLabel("AO Stim Channel:", tmphb);
    ao_channels = new SearchableComboBox(tmphb);
    (void) new QLabel("    " /* poor man's spaceing */, tmphb);

    populateAOComboBox();
    synchAOChan();
    connect(ao_channels, SIGNAL(activated(const QString &)), this, SLOT(changeAOChan(const QString &)));
       
    (void) new QLabel("Nominal Num. Stims:", tmphb);
    QSpinBox *nomStimsSpinBox = new  QSpinBox (MC_MIN_NUM_STIMS, MC_MAX_NUM_STIMS, 1, tmphb);
    nomStimsSpinBox->setValue(shm->nom_num_stims);

    connect(nomStimsSpinBox, SIGNAL(valueChanged(int)), this, SLOT(changeNomNumStims(int)));

    tmphb = new QHBox(tmpvb);
    tmphb->setSpacing(6);

    (void) new QLabel("Value of g:", tmphb);
    gval = new QLineEdit(tmphb);
    gval->setValidator(new QDoubleValidator(gval));
    gval->setText(QString("%1").arg(shm->g_val));
    connect(gval, SIGNAL(textChanged(const QString &)), this, SLOT(changeG(const QString &)));
    (void) new QLabel("    ", tmphb); // po' man's spacing

    g_adj_manual_only = new QCheckBox("Adjust g Only Manually", tmphb);

    g_adj_manual_only->setChecked(shm->g_adjustment_mode == MC_G_ADJ_MANUAL);
    g_adj_manual_only->setDisabled(!shm->stim_on);
    gval->setDisabled(!g_adj_manual_only->isChecked() && !shm->stim_on);
    connect(g_adj_manual_only, SIGNAL(stateChanged(int)), this, SLOT(gAdjManualOnly(int)));

    control_toggle = new QCheckBox("Control On", tmphb);

    control_toggle->setChecked(shm->stim_on);    

    connect(control_toggle, SIGNAL(toggled(bool)), this, SLOT(toggleControl(bool)));
   


  
    /* delta-g controls */
    tmphb = new QHBox(tmpvb);

    tmphb->setSpacing(5);
    
    (void) new QLabel("Delta-g:", tmphb);

   
    delta_g_bar_value = new QLabel(QString::number(shm->delta_g), tmphb);
    delta_g_bar = new QScrollBar
      ( mc_delta_g_toint(MC_DELTA_G_MIN), mc_delta_g_toint(MC_DELTA_G_MAX),
        1, mc_delta_g_toint((MC_DELTA_G_MAX - MC_DELTA_G_MIN) / 5.0), 
        mc_delta_g_toint(shm->delta_g), Qt::Horizontal, tmphb );
    
    tmphb->setStretchFactor(delta_g_bar, 1);
    
    /* num_rr_avg controls */
    (void) new QLabel("  No. RR Avg:", tmphb);

    num_rr_avg_bar_value = new QLabel(QString::number(shm->num_rr_avg), tmphb);
    num_rr_avg_bar = new QScrollBar
      ( MC_NUM_RR_AVG_MIN, MC_NUM_RR_AVG_MAX, 1, 
        (MC_NUM_RR_AVG_MAX - MC_NUM_RR_AVG_MIN) / 10, shm->num_rr_avg,
        Qt::Horizontal, tmphb );  

    tmphb->setStretchFactor(num_rr_avg_bar, 1);
   

    connect(delta_g_bar, SIGNAL(valueChanged(int)), this, SLOT(changeDG(int)));
    connect(num_rr_avg_bar, SIGNAL(valueChanged(int)), this, SLOT(changeNumRRAvg(int)));
    connect(num_rr_avg_bar, SIGNAL(valueChanged(int)), num_rr_avg_bar_value, SLOT(setNum(int)));
  }
  
  controlslayout->addMultiCellWidget // footnote at the bottom
    (new QLabel(QString("MC Control Experiment Notes:\n"
                        "All graphs display %1 beats on the x-axis.\n"
                        "The vertical gridlines are %2 beats apart.")
               .arg(beats_per_gridline * num_gridlines)
               .arg(beats_per_gridline), controls), 
     n_ctl_r-1, n_ctl_r-1, 0, n_ctl_c-1, AlignBottom | AlignLeft);


  window_id = daqSystem()->windowMenuAddWindow(window);  
  window->show();


  timer = new QTimer(this);
  connect(timer, SIGNAL(timeout()), this, SLOT(periodic()));
  timer->start(50);
}

MapControl::~MapControl() 
{
  timer->stop();
  safelyQuit(); /* prompts the user to save, if necessary.. kinda sloppy to 
                   put this in a destructor.. but 'oh well' */
  daqSystem()->windowMenuRemoveWindow(window_id);
  if (shm)  shm->spike_channel = -1; // to 'turn off' avn stim...
  moduleDetach();  
  delete window;
  delete spooler;
}

const char *
MapControl::name() const
{
  return ::name;
}

const char *
MapControl::description() const
{
  return ::description;
}


void MapControl::determineRTOS()
{
  rtosUsed = Unknown;
  if (QFile::exists("/proc/rtai")) rtosUsed = RTAI;
  else if (QFile::exists("/dev/rtl_shm")) rtosUsed = RTLinux;// temp hack

  Assert<PluginException>(rtosUsed != Unknown,
                          "Map Control Plugin Attach Error",
                          "The Map Control plugin could not be attached "
                          "because it can't figure out which RTOS you are "
                          "running.  Are you running either RTLinux or RTAI?");
}

void MapControl::moduleAttach()
{
  switch(rtosUsed) {
  case RTLinux:
    shm = (MCShared *)mbuff_attach(MC_SHM_NAME, sizeof(struct MCShared));
    break;
  case RTAI:
    shm = (MCShared *)rtai_shm_attach(MC_SHM_NAME, sizeof(struct MCShared));
    break;
  default:
    /* Should not be reached */
    throw PluginException("Internal Error", QString("Internal error on line ")
                          + QString::number(__LINE__) + " in file " 
                          + __FILE__);
    break;
  }

  Assert<PluginException>(shm && shm->magic == MC_SHM_MAGIC,
                          "Map Control Plugin Attach Error", 
                          "The Map Control Plugin can not find the shared "
                          "memory buffer named \"" MC_SHM_NAME "\".  "
                          "(Or it is of the wrong version)\n\n"
                          "Are you sure that module map_control.o is loaded?");
  

  QString fname;

  fname.sprintf("%s%d","/dev/rtf",shm->fifo_minor);
  fifo = open(fname, O_RDONLY | O_NDELAY);

  Assert<PluginException>(!needFifo(),
                          "Map Control Plugin Attach Error",
                          QString 
                          ( "Even though the Map Control Kernel Module is "
                            "loaded, the Map Control plugin could not attach\n "
                            "to the required fifo \"%1\".  Please make sure "
                            "this file exists and is read/write for the \n"
                            "current user."
                            ).arg(fname));
  
  /* empty the fifo to make sure we start with a clean slate */
  char buf[MC_FIFO_SZ];
  ::read(fifo, &buf, MC_FIFO_SZ);
    
}

void MapControl::moduleDetach()
{
  if (!needFifo()) close(fifo); fifo = -1;

  if (shm) {

    switch(rtosUsed) {
    case RTLinux:
      mbuff_detach(MC_SHM_NAME, shm);
      break;
    case RTAI:
      rtai_shm_detach(MC_SHM_NAME, shm);
      break;
    default:
      /* Should not be reached */
      throw PluginException("Internal Error", 
                            QString("Internal error on line ") 
                            + QString::number(__LINE__) + " in file " 
                            + __FILE__);
      break;
    }

    shm = 0;

  }
}

/* returns parent() dynamic_cast to DAQSystem* */
DAQSystem * MapControl::daqSystem()  
{ return dynamic_cast<DAQSystem *>(parent()); }


/* Very long-winded code to build the graph axis labels... */
void MapControl::addAxisLabels()
{
  /* add the axis labels */
  QGridLayout *tmp_lo;
  QWidget *w;
  QLabel *tmp_l; // temporary pointer to axis labels
  QFont f; f.setPointSize(8); // label point size

  /* RR Interval Axis Labels */
  w = new QWidget(graphs); // dummy widget to have nested grids
  graphlayout->addWidget(w, 1, 0);

  tmp_lo = new QGridLayout(w, 3, 1);

  tmp_lo->addWidget(tmp_l = new QLabel(QString().setNum(rr_graph->rangeMax()), w), 0, 0, AlignTop | AlignRight); 
  tmp_l->setFont(f);

  tmp_lo->addWidget(tmp_l = new QLabel(QString().setNum((rr_graph->rangeMax() - rr_graph->rangeMin()) / 2.0 + rr_graph->rangeMin()), w), 1, 0, AlignVCenter | AlignRight);
  tmp_l->setFont(f);

  tmp_lo->addWidget(tmp_l = new QLabel(QString().setNum(rr_graph->rangeMin()), w), 2, 0, AlignBottom | AlignRight);
  tmp_l->setFont(f);

  /* Stim Graph Axis Labels */
  w = new QWidget(graphs); // dummy widget to have nested grids
  graphlayout->addWidget(w, 3, 0);

  tmp_lo = new QGridLayout(w, 3, 1);

  tmp_lo->addWidget(tmp_l = new QLabel(QString().setNum(stim_graph->rangeMax()), w), 0, 0, AlignTop | AlignRight); 
  tmp_l->setFont(f);

  tmp_lo->addWidget(tmp_l = new QLabel(QString().setNum((stim_graph->rangeMax() - stim_graph->rangeMin()) / 2.0 + stim_graph->rangeMin()), w), 1, 0, AlignVCenter | AlignRight);
  tmp_l->setFont(f);

  tmp_lo->addWidget(tmp_l = new QLabel(QString().setNum(stim_graph->rangeMin()), w), 2, 0, AlignBottom | AlignRight);
  tmp_l->setFont(f);

  /* G Graph Axis Labels */
  w = new QWidget(graphs); // dummy widget to have nested grids
  graphlayout->addWidget(w, 5, 0);

  tmp_lo = new QGridLayout(w, 3, 1);

  tmp_lo->addWidget(tmp_l = new QLabel(QString().setNum(g_graph->rangeMax()), w), 0, 0, AlignTop | AlignRight); 
  tmp_l->setFont(f);

  tmp_lo->addWidget(tmp_l = new QLabel(QString().setNum((g_graph->rangeMax() - g_graph->rangeMin()) / 2.0 + g_graph->rangeMin()), w), 1, 0, AlignVCenter | AlignRight);
  tmp_l->setFont(f);

  tmp_lo->addWidget(tmp_l = new QLabel(QString().setNum(g_graph->rangeMin()), w), 2, 0, AlignBottom | AlignRight);
  tmp_l->setFont(f);

}

void MapControl::periodic()
{
  /* check the in fifo for data, and act upon it if found... */
  readInFifo();

  /* synch the ao_channels listbox with what is really in the kernel */
  synchAOChan();    

}

void MapControl::readInFifo()
{
  static const int n2rd = 1000; /* number of MCSnapShot's to read at a time */
  MCSnapShot buf[n2rd];
  int n_read = 0, n_bufs = 0, i = 0;

  n_read = ::read(fifo, buf, sizeof(buf));

  n_read = ( n_read < 0 ? 0 : n_read ); // rtai fifos return -1 on failure

  n_bufs = n_read / sizeof(MCSnapShot);

  need_to_save = (n_bufs ? true : need_to_save);
  
  for (i = 0; i < n_bufs; i++) {
    /* todo: plot to graphs here... */
    rr_graph->plot(static_cast<double>(buf[i].rr_interval));
    stim_graph->plot(static_cast<double>(buf[i].stimuli));
    g_graph->plot(buf[i].g_val);
    spooler->spool(buf, n_bufs);
  }
  /* now update the stats once per read() call (meaning we take the
     last MCSnapShot we got */
  if (!n_bufs) return;
  
  MCSnapShot & last = buf[n_bufs-1];

  current_rri->setText(QString::number(last.rr_interval));
  current_rrt->setText(QString::number(last.rr_target));
  current_stim->setText(QString::number(last.stimuli));
  current_nom_stim->setText(QString::number(last.nom_num_stims));
  current_g->setText(QString::number(last.g_val));
  if (!g_adj_manual_only->isChecked()) 
    gval->setText(QString::number(last.g_val));
  last_beat_index->setText(QString(uint64_to_cstr(last.scan_index))); 
  current_rr_avg->setText(QString::number(last.rr_avg));
  current_num_rr_avg->setText(QString::number(last.num_rr_avg));
  current_g2small->setText(QString::number(last.g_too_small_count));
  current_g2big->setText(QString::number(last.g_too_big_count));
  current_delta_g->setText(QString::number(last.delta_g));
  /* update our slider value representation if actual last read delta_g
     disagrees with the UI's notion of what it is... */
  static const float smallf = 0.0001;
#define f_equal(x, y) ( x > y ? x - y < smallf : y - x < smallf )
  if (!f_equal(last.delta_g, delta_g_bar_value->text().toFloat())) {
    disconnect(delta_g_bar, SIGNAL(valueChanged(int)), this, SLOT(changeDG(int)));
    delta_g_bar->setValue(mc_delta_g_toint(last.delta_g));
    delta_g_bar_value->setText(QString::number(last.delta_g, 'g', 3));
    connect(delta_g_bar, SIGNAL(valueChanged(int)), this, SLOT(changeDG(int)));
  }

  /* update our slider value representation if actual last read num_rr_avg
     disagrees with the UI's notion of what it is... */
  if (last.num_rr_avg != num_rr_avg_bar_value->text().toInt()) {
    disconnect(num_rr_avg_bar, SIGNAL(valueChanged(int)), this, SLOT(changeNumRRAvg(int)));
    num_rr_avg_bar->setValue(last.num_rr_avg);
    connect(num_rr_avg_bar, SIGNAL(valueChanged(int)), this, SLOT(changeNumRRAvg(int)));          
  }

  

  /* incomplete... please finish! - Calin */
#undef f_equal
}

/* Possible race conditions here but it's too much trouble to implement
   atomicity for the free channels list right now... */
void MapControl::populateAOComboBox()
{
  uint i;

  ao_channels->clear();

  for (i = 0; i < daq_shmCtl->numChannels(ComediSubDevice::AnalogOutput); i++)
    if (!daq_shmCtl->isChanOn(ComediSubDevice::AnalogOutput, i) 
        || i == static_cast<uint>(shm->ao_chan))
      ao_channels->insertItem(QString::number(i));
  
}

/* possible race conditions here.. since channel free list access is
   non-atomic! */
void MapControl::changeAOChan(const QString & selected)
{
  int selected_i = selected.toInt();
  
  /* disconnect here to avoid infinite recursion */
  disconnect(ao_channels, SIGNAL(activated(const QString &)), this, SLOT(changeAOChan(const QString &)));
  populateAOComboBox(); /* make sure the channel still is open --
                          possible race condition here! */

  /* if it's still there, tell the kernel about our desire to change
     the channel */
  if (ao_channels->findItem(selected, CaseSensitive|ExactMatch) >= 0) {
    shm->ao_chan = selected_i;
    synchAOChan(); /* just for kicks - this may revert back for a split
                      second as synchAOChan() in periodic() */
  }
  connect(ao_channels, SIGNAL(activated(const QString &)), this, SLOT(changeAOChan(const QString &)));
}

/* synch the ao_channels listbox with what is really in the kernel */
void MapControl::synchAOChan() 
{
  QString ao_chan;

  ao_chan.setNum(shm->ao_chan);

  if (ao_channels->currentText() != ao_chan) 
    ao_channels->setSelected(ao_channels->findItem(ao_chan, CaseSensitive|ExactMatch), true); 
}

void MapControl::changeAIChan(const QString &chan)
{
  bool ok;

  shm->spike_channel = chan.toInt(&ok);

  if (!ok) shm->spike_channel = -1;
}

void MapControl::synchAIChan()
{
  uint i;
  int sel;

  disconnect(ai_channels, SIGNAL(activated(const QString &)), this, SLOT(changeAIChan(const QString &)));

 ai_channels->clear();
  ai_channels->insertItem("Off");
  for (i = 0; i < daq_shmCtl->numChannels(ComediSubDevice::AnalogInput); i++)
    if (daq_shmCtl->isChanOn(ComediSubDevice::AnalogInput, i))
      ai_channels->insertItem(QString().setNum(i));
  
  sel = ai_channels->findItem(QString().setNum(shm->spike_channel), CaseSensitive | ExactMatch);
  if (sel < 0) {
    ai_channels->setSelected(0);
    shm->spike_channel = -1;
  }
  else ai_channels->setSelected(sel); 

  connect(ai_channels, SIGNAL(activated(const QString &)), this, SLOT(changeAIChan(const QString &)));

}

void MapControl::toggleControl(bool on)
{
  
  if (!on) {
    shm->stim_on = 0;
    g_adj_manual_only->setDisabled(true);
    gval->setDisabled(true);
    shm->g_adjustment_mode = MC_G_ADJ_MANUAL;
  } else {
    shm->stim_on = 1;
    g_adj_manual_only->setDisabled(false);
    gval->setDisabled(!g_adj_manual_only->isChecked());
    shm->g_adjustment_mode = (g_adj_manual_only->isChecked() 
                              ? MC_G_ADJ_MANUAL 
                              : MC_G_ADJ_AUTOMATIC);
  }
    
}



void MapControl::changeRRTarget(int rrt_in_ms)
{
  shm->rr_target = rrt_in_ms;
}

void MapControl::changeNumRRAvg(int rra)
{
  shm->num_rr_avg = rra;
}

void MapControl::gAdjManualOnly(int yes)
{
  shm->g_adjustment_mode = (yes ? MC_G_ADJ_MANUAL : MC_G_ADJ_AUTOMATIC);
  gval->setDisabled(!yes);
}

void MapControl::changeG(const QString &g)
{  
  double newG;
  bool ok;

  newG = g.toDouble(&ok);
  if (ok)
    shm->g_val = newG;
}

void MapControl::changeDG(int new_dg_sliderval) 
{
  shm->delta_g = mc_delta_g_fromint(new_dg_sliderval);
  delta_g_bar_value->setNum(mc_delta_g_fromint(new_dg_sliderval));
}

void MapControl::changeNomNumStims(int n)
{
  shm->nom_num_stims = n;
}

const char * MapControl::fileheader = 
"#File Format Version: " RCS_VERSION_STRING "\n"
"#--\n#Columns: \n"
"#Scan_Index RR_Interval Number_of_Stimuli G RR_Avg G_Too_Small_Count G_Too_Big_Count RR_Target Delta_G Num_RR_Avg Stim_On? G_Adj_Automatically?\n";


struct PVSaver
{  
  PVSaver (QTextStream & o, QString header = "") 
    : out(o), header(header) { out << header; };
  
  void operator()(MCSnapShot & l) 
  { 
    dummy.sprintf
      ("%s %d %d %g %d %d %d %d %g %d %d %d\n",     
       uint64_to_cstr(l.scan_index), l.rr_interval, l.stimuli, l.g_val,
       l.rr_avg, l.g_too_small_count, l.g_too_big_count, l.rr_target, 
       (double)l.delta_g, l.num_rr_avg, (int)l.stim_on, (int)l.g_adjustment_mode);
    out << dummy;
  }

  QTextStream &out;
  QString dummy, header;
};

void MapControl::save()
{ 
  if (outFile.isNull()) { saveAs(); return; }

  QFile f(outFile);

  if (!f.open(IO_WriteOnly | IO_Translate | IO_Truncate)) {
    QMessageBox::critical(window, outFile + " cannot be opened!", 
                          QString("Could not open ") + outFile + " for writing"
                          ", please choose another file.", QMessageBox::Ok, 0);
    saveAs();
    return;
  }

  QTextStream out(&f);
  out.setEncoding(QTextStream::Latin1);
  
  {
    PVSaver saver(out, fileheader);
    spooler->forEach(saver);
  }

  f.flush();
  if ( (f.status() & IO_Ok) != IO_Ok ) {
    Exception("Error writing to outfile", "There was an unspecified error "
              "while saving data to the output file.  "
              "Please choose another file.").showError();
    saveAs();
    return;
  }

  need_to_save = false;
}

void MapControl::saveAs()
{
  bool dont_overwrite = false;

  do {
    outFile =  
      QFileDialog::getSaveFileName(outFile, QString::null, window, 
                                   "save file dialog",
                                   "Choose a file to which to save the"
                                   " MC data" );

    if (outFile.isNull()) return; // user aborted

    if (QFile(outFile).exists()) {
      dont_overwrite = 
        QMessageBox::warning(window, "File exists", 
                             outFile + " exists, overwrite?", 
                             QMessageBox::Yes, QMessageBox::No) == QMessageBox::No;
    } else {
      dont_overwrite = false;
    }
  } while (dont_overwrite);
  save();
}

void MapControl::safelyQuit()
{
  if (need_to_save) {
    int usersaid = 
      QMessageBox::warning(window, "Data is unsaved", "The Map Control Plugin is terminating, and the data for the Map Control Control Experiment is unsaved.\nDo you wish to save or discard changes?", "Save Changes", "Discard Changes");
    if (usersaid == 0)
      save(); /* this way even on file pick error
                 they will be prompted to save */
  }
}

#undef spooler
