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
#include "apd_control.h"

#include "apd_control_private.h"
#include "searchable_combo_box.h"

#define RCS_VERSION_STRING  "$Id$"


extern "C" {

  /* Stuff needed by plugin engine... */
  int ds_plugin_ver = DS_PLUGIN_VER;

  const char * name = "APD Control Experiment",
             * description =
              "Some GUI controls for interfacing with the Apd Control stimulation "
              "real-time module named 'apd_control.o'.  This rtl/rt_process "
              "addon was developed for a cardiac action potential control "
              "at Cornell University.",
             * author =
              "David J. Christini, Ph.D and Calin A. Culianu.",
             * requires =
              "apd_control.o be installed into the kernel.  Analog output.";
  
  Plugin * entry(QObject *o) 
  { 

    /* Top-level widget.. parent is root */
    DAQSystem *d = dynamic_cast<DAQSystem *>(o);

    Assert<PluginException>(d, "APD Control Plugin Load Error", 
                            "The APD Control Plugin can only be used in "
                            "conjunction with daq_system!  Sorry!");    
    return new APDcontrol(d); ;
  } 

};

/*************************************************************/
/* edit this section to change graph axes, grids, etc. */
/*************************************************************/
/* default values for the graphs */
static const int beats_per_gridline = 100;
static const int num_gridlines = 5;

/* the following struct and the static arrays of that struct are used
   to populate the graph axes controls.  These correspond to 
   member variables of this class (see apd_control_private.h) of type 
   ECGGraph *  

   Here is the definition of this struct, as found in apd_control_private.h:

   struct APDcontrol::GraphRangeSettings {  double min;  double max;  };
*/

const int APDcontrol::n_apd_ranges = 5;
const APDcontrol::GraphRangeSettings APDcontrol::apd_ranges[n_apd_ranges] =
  { 
    { min: 50, max: 180 }, { min: 0, max: 200 }, { min: 0, max: 400 },
    { min: -10, max: 600 }, { min: 0,  max: 1000 } 
  };

const int APDcontrol::n_delta_pi_ranges = 4;
const APDcontrol::GraphRangeSettings APDcontrol::delta_pi_ranges[n_delta_pi_ranges] = 
  {
    { min: -50, max: 50 }, { min: -75, max: 75 }, { min: -100, max: 100},
    { min: -200, max: 200 }
  };

const int APDcontrol::n_g_ranges = 4;
const APDcontrol::GraphRangeSettings APDcontrol::g_ranges[n_g_ranges] = 
  {
    { min: 0, max: 1 }, { min: 0, max: 5 }, { min: 0, max: 10},
    { min: 0, max: 20 }
  };

/********************************************************************/
/*
 *MAIN OBJECT*
 edit this function to change the GUI look and feel
 Most of your edits should probably be in this function
*/
/********************************************************************/
APDcontrol::APDcontrol(DAQSystem *daqSystem_parent) 
  : QObject(daqSystem_parent, ::name), need_to_save(false),  
    shm(NULL), fifo(-1)
{
  daq_shmCtl = &daqSystem_parent->shmController();

  spooler = new TempSpooler<MCSnapShot>("apdcontrol", true);

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
  graphs = new QWidget(window, "APD Control Graph Virtual Widget");
  masterlayout->addWidget(graphs, 1, 1);
  masterlayout->setColStretch(1, 2);

  graphlayout = new QGridLayout(graphs, 6, 3);
  graphlayout->setColStretch(1, 1);
  
  apd_graph   = new ECGGraph (beats_per_gridline, num_gridlines, 
                             apd_ranges[0].min, apd_ranges[0].max, 
                             graphs, QString(name()) + " - RR Interval");
  delta_pi_graph = new ECGGraph (beats_per_gridline, num_gridlines, 
                             delta_pi_ranges[0].min, delta_pi_ranges[0].max, 
                             graphs, QString(name()) + " - Number of Stimuli");
  
  g_graph    = new ECGGraph (beats_per_gridline, num_gridlines, 
                             g_ranges[0].min, g_ranges[0].max,
                             graphs, QString(name()) + " - G Values");

  g_graph->plotFactor = delta_pi_graph->plotFactor = apd_graph->plotFactor = 2;

  graphlayout->addMultiCellWidget(apd_graph, 1, 1, 1, 2); 
  graphlayout->setRowStretch(1, 3);
  graphlayout->addMultiCellWidget(delta_pi_graph, 3, 3, 1, 2); 
  graphlayout->setRowStretch(3, 3);
  graphlayout->addMultiCellWidget(g_graph, 5, 5, 1, 2); 
  graphlayout->setRowStretch(5, 3);
  
  graphlayout->addWidget(new QLabel("APD", graphs), 0, 1);
  graphlayout->addWidget(new QLabel("Delta PI", graphs), 2, 1);
  graphlayout->addWidget(new QLabel("g", graphs), 4, 1);

  apd_range_ctl = new QComboBox(false, graphs, "APD Range Combo box");
  delta_pi_range_ctl = new QComboBox(false, graphs, "Delta Pi Range Combo box");
  g_range_ctl = new QComboBox(false, graphs, "G Range Combo box");

  graphlayout->addWidget(apd_range_ctl, 0, 2);
  graphlayout->addWidget(delta_pi_range_ctl, 2, 2);
  graphlayout->addWidget(g_range_ctl, 4, 2);  

  addAxisLabels();

  buildRangeComboBoxesAndConnectSignals();
  /* end graphs... */

/*******************************************************/
/* edit this section to change text displays etc. */
/*******************************************************/

  controls = new QWidget(window);
  masterlayout->addWidget(controls, 1, 0);

  const int n_ctl_r = 3, n_ctl_c = 1;
  controlslayout = new QGridLayout(controls, n_ctl_r, n_ctl_c);

  { /* stats groupbox */
    QGroupBox *stats = new QGroupBox(1, Vertical, "Stats", controls);
    controlslayout->addMultiCellWidget(stats, 0, 0, 0, n_ctl_c-1);

    QWidget *tmpw = new QWidget(stats);

    QGridLayout *tmplo = new QGridLayout(tmpw, 11, 4);

    // text displays
    int line_counter=0;
    tmplo->addWidget(new QLabel("APD_n (ms): ", tmpw), line_counter++, 0);
    tmplo->addWidget(new QLabel("DI_n: ", tmpw), line_counter++, 0);
    tmplo->addWidget(new QLabel("PI = APD+DI(ms): ", tmpw), line_counter++, 0);
    tmplo->addWidget(new QLabel("delta PI (ms): ", tmpw), line_counter++, 0);
    tmplo->addWidget(new QLabel("APA (V): ", tmpw), line_counter++, 0);
    tmplo->addWidget(new QLabel("AP baseline (V): ", tmpw), line_counter++, 0);
    line_counter=0;
    tmplo->addWidget(new QLabel("AP_ti (ms): ", tmpw), line_counter++, 2);
    tmplo->addWidget(new QLabel("AP_tf (ms): ", tmpw), line_counter++, 2);
    tmplo->addWidget(new QLabel("g: ", tmpw), line_counter++, 2);
    tmplo->addWidget(new QLabel("delta g: ", tmpw), line_counter++, 2);
    tmplo->addWidget(new QLabel("alternating perturbations: ", tmpw), line_counter++, 2);
    
    line_counter=0;
    tmplo->addWidget(gui_indicator_apd = new QLabel(tmpw), line_counter++, 1); 
    tmplo->addWidget(gui_indicator_di = new QLabel(tmpw), line_counter++, 1);    
    tmplo->addWidget(gui_indicator_pi = new QLabel(tmpw), line_counter++, 1);
    tmplo->addWidget(gui_indicator_delta_pi = new QLabel(tmpw), line_counter++, 1);
    tmplo->addWidget(gui_indicator_v_apa = new QLabel(tmpw), line_counter++, 1);
    tmplo->addWidget(gui_indicator_v_baseline = new QLabel(tmpw), line_counter++, 1);
    line_counter=0;
    tmplo->addWidget(gui_indicator_ap_ti = new QLabel(tmpw), line_counter++, 3);
    tmplo->addWidget(gui_indicator_ap_tf = new QLabel(tmpw), line_counter++, 3);
    tmplo->addWidget(gui_indicator_g_val = new QLabel(tmpw), line_counter++, 3);
    tmplo->addWidget(gui_indicator_delta_g = new QLabel(tmpw), line_counter++, 3);
    tmplo->addWidget(gui_indicator_consec_alternating = new QLabel(tmpw), line_counter++, 3);

  }

/*******************************************************/
/* edit this section to change GUI controls      
   note that to change the functionality of a 
   particular control, you have to change its
   slot function directly                                        */
/*******************************************************/
  
  { /* controls groupbox */
    QGroupBox *ctls = new QGroupBox(1, Vertical, "Controls", controls);
    controlslayout->addMultiCellWidget(ctls, 1, 1, 0, n_ctl_c-1);

    //new line of control widgets
    QVBox *tmpvb = new QVBox(ctls);
    QHBox *tmphb = new QHBox(tmpvb);
    tmphb->setSpacing(5);

    (void)new QLabel("AI channel to measure APDs:", tmphb);

    ai_channels = new SearchableComboBox(tmphb);
    connect(ai_channels, SIGNAL(activated(const QString &)), this, SLOT(changeAIChan(const QString &)));
    connect(daqSystem(), SIGNAL(channelOpened(uint)), this, SLOT(synchAIChan()));
    connect(daqSystem(), SIGNAL(channelClosed(uint)), this, SLOT(synchAIChan()));
    synchAIChan();
    tmphb->setStretchFactor(new QWidget(tmphb), 1);     /* Invisible spacer widget    |-------------| */

    (void) new QLabel("APD90,80,70,...:", tmphb);
    apd_xx_edit_box = new QLineEdit(tmphb);
    apd_xx_edit_box->setValidator(new QIntValidator(apd_xx_edit_box));
    apd_xx_edit_box->setText(QString("%1").arg( (int)(100*(1.0-(shm->apd_xx)))) );
    apd_xx_edit_box->setMaxLength(3);
    connect(apd_xx_edit_box, SIGNAL(textChanged(const QString &)), this, SLOT(changeAPDxx(const QString &)));
    apd_xx_edit_box->setMaximumWidth(apd_xx_edit_box->minimumSizeHint().width());

    //new line of control widgets
    tmphb = new QHBox(tmpvb);        
    tmphb->setSpacing(5);

    (void)new QLabel("AO stim channel:", tmphb);
    ao_channels = new SearchableComboBox(tmphb);
    tmphb->setStretchFactor(new QWidget(tmphb), 1);     /* Invisible spacer widget    |-------------| */
    populateAOComboBox();
    synchAOChan();
    connect(ao_channels, SIGNAL(activated(const QString &)), this, SLOT(changeAOChan(const QString &)));

    //new line of control widgets
    tmphb = new QHBox(tmpvb);        
    tmphb->setSpacing(5);

    pacing_toggle = new QCheckBox("Pacing ON", tmphb);
    pacing_toggle->setChecked(shm->pacing_on);    
    connect(pacing_toggle, SIGNAL(toggled(bool)), this, SLOT(togglePacing(bool)));
    tmphb->setStretchFactor(new QWidget(tmphb), 1);     /* Invisible spacer widget    |-------------| */
    
    (void) new QLabel("Nominal PI:", tmphb);
    nominal_pi = new QSpinBox(1, 2000, 1, tmphb);    
    nominal_pi->setValue(static_cast<int>(shm->nominal_pi));
    connect(nominal_pi, SIGNAL(valueChanged(int)), this, SLOT(changeNominalPI(int)));
    
    //new line of control widgets
    tmphb = new QHBox(tmpvb);
    tmphb->setSpacing(5);

    control_toggle = new QCheckBox("Control ON", tmphb);
    control_toggle->setChecked(shm->control_on);    
    connect(control_toggle, SIGNAL(toggled(bool)), this, SLOT(toggleControl(bool)));

    underlying_toggle = new QCheckBox("Continue underlying pacing", tmphb);
    underlying_toggle->setChecked(shm->continue_underlying);    
    connect(underlying_toggle, SIGNAL(toggled(bool)), this, SLOT(toggleUnderlying(bool)));

    //new line of control widgets
    tmphb = new QHBox(tmpvb);
    tmphb->setSpacing(5);

    only_negative_toggle = new QCheckBox("Only negative perturbations", tmphb);
    only_negative_toggle->setChecked(shm->only_negative_perturbations);    
    connect(only_negative_toggle, SIGNAL(toggled(bool)), this, SLOT(toggleOnlyNegativePerturbations(bool)));

    target_shorter_toggle = new QCheckBox("Target shorter APD initially", tmphb);
    target_shorter_toggle->setChecked(shm->target_shorter);    
    connect(target_shorter_toggle, SIGNAL(toggled(bool)), this, SLOT(toggleTargetShorter(bool)));

    //new line of control widgets
    tmphb = new QHBox(tmpvb);
    tmphb->setSpacing(5);

    (void) new QLabel("g: ", tmphb);
    gval = new QLineEdit(tmphb);
    gval->setValidator(new QDoubleValidator(gval));
    gval->setText(QString("%1").arg(shm->g_val));
    connect(gval, SIGNAL(textChanged(const QString &)), this, SLOT(changeG(const QString &)));


    g_adj_manual_only = new QCheckBox("Adjust g Only Manually", tmphb);
    g_adj_manual_only->setChecked(shm->g_adjustment_mode == MC_G_ADJ_MANUAL);
    g_adj_manual_only->setDisabled(!shm->control_on);
    //gval->setDisabled(!g_adj_manual_only->isChecked() && !shm->control_on);
    connect(g_adj_manual_only, SIGNAL(stateChanged(int)), this, SLOT(gAdjManualOnly(int)));

    (void)new QLabel("delta g:", tmphb);
    /* delta-g controls */   
    gui_indicator_delta_g_bar_value = new QLabel(QString::number(shm->delta_g), tmphb);
    delta_g_bar = new QScrollBar
      ( mc_delta_g_toint(MC_DELTA_G_MIN), mc_delta_g_toint(MC_DELTA_G_MAX),
        1, mc_delta_g_toint((MC_DELTA_G_MAX - MC_DELTA_G_MIN) / 5.0), 
        mc_delta_g_toint(shm->delta_g), Qt::Horizontal, tmphb );
     connect(delta_g_bar, SIGNAL(valueChanged(int)), this, SLOT(changeDG(int)));
     delta_g_bar->setSizePolicy(QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed)); 
     delta_g_bar->setMinimumSize(100, delta_g_bar->minimumSize().height()); // force this scrollbar to EXIST!
     tmphb->setStretchFactor(delta_g_bar, 3);
  }
  
  controlslayout->addMultiCellWidget // footnote at the bottom
    (new QLabel(QString("APD control experiment notes:\n"
                        "x-axis displays %1 points (%2 points per vertical gridline).")
               .arg(beats_per_gridline * num_gridlines)
               .arg(beats_per_gridline), controls), 
     n_ctl_r-1, n_ctl_r-1, 0, n_ctl_c-1, AlignBottom | AlignLeft);

  window_id = daqSystem()->windowMenuAddWindow(window);  
  window->show();

  timer = new QTimer(this);
  connect(timer, SIGNAL(timeout()), this, SLOT(periodic()));
  timer->start(50);
}


/********************************************************************/
/*
Most of the stuff below here won't need major editing.
Notable exceptions are: (1) the slot functions for your GUI 
controls, (2) readInFifo, and (3) PVSaver
*/
/********************************************************************/

//destructor
APDcontrol::~APDcontrol() 
{
  timer->stop();
  safelyQuit(); /* prompts the user to save, if necessary.. kinda sloppy to 
                   put this in a destructor.. but 'oh well' */
  daqSystem()->windowMenuRemoveWindow(window_id);
  if (shm) shm->pacing_on = 0; // to 'turn off' pacing ...
  moduleDetach();  
  delete window;
  delete spooler;
}

const char *
APDcontrol::name() const
{
  return ::name;
}

const char *
APDcontrol::description() const
{
  return ::description;
}


void APDcontrol::determineRTOS()
{
  rtosUsed = Unknown;
  if (QFile::exists("/proc/rtai")) rtosUsed = RTAI;
  else if (QFile::exists("/dev/rtl_shm")) rtosUsed = RTLinux;// temp hack

  Assert<PluginException>(rtosUsed != Unknown,
                          "APD Control Plugin Attach Error",
                          "The APD Control plugin could not be attached "
                          "because it can't figure out which RTOS you are "
                          "running.  Are you running either RTLinux or RTAI?");
}

void APDcontrol::moduleAttach()
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
                          "APD Control Plugin Attach Error", 
                          "The APD Control Plugin can not find the shared "
                          "memory buffer named \"" MC_SHM_NAME "\".  "
                          "(Or it is of the wrong version)\n\n"
                          "Are you sure that module apd_control.o is loaded?");
  

  QString fname;

  fname.sprintf("%s%d","/dev/rtf",shm->fifo_minor);
  fifo = open(fname, O_RDONLY | O_NDELAY);

  Assert<PluginException>(!needFifo(),
                          "APD Control Plugin Attach Error",
                          QString 
                          ( "Even though the APD Control Kernel Module is "
                            "loaded, the APD Control plugin could not attach\n "
                            "to the required fifo \"%1\".  Please make sure "
                            "this file exists and is read/write for the \n"
                            "current user."
                            ).arg(fname));
  
  /* empty the fifo to make sure we start with a clean slate */
  char buf[MC_FIFO_SZ];
  ::read(fifo, &buf, MC_FIFO_SZ);
    
}

void APDcontrol::moduleDetach()
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
DAQSystem * APDcontrol::daqSystem()  
{ return dynamic_cast<DAQSystem *>(parent()); }


/* Very long-winded code to build the graph axis labels... */
void APDcontrol::addAxisLabels(bool onlyNull)
{
  /* add the axis labels */
  QGridLayout *tmp_lo;
  QLabel *tmp_l; // temporary pointer to axis labels
  QFont f; f.setPointSize(8); // label point size


  QWidget *w; /* tmp working wiget pointer points to members:
                 apd_graph_labels, delta_pi_graph_labels, etc.. */

  if (!onlyNull || !apd_graph_labels) {

    /* APD Graph Axis Labels */

    w = apd_graph_labels = new QWidget(graphs); // dummy widget to have nested grids

    graphlayout->addWidget(w, 1, 0);
    
    tmp_lo = new QGridLayout(w, 3, 1);

    tmp_lo->addWidget(tmp_l = new QLabel(QString().setNum(apd_graph->rangeMax()), w), 0, 0, AlignTop | AlignRight); 
    tmp_l->setFont(f);

    tmp_lo->addWidget(tmp_l = new QLabel(QString().setNum((apd_graph->rangeMax() - apd_graph->rangeMin()) / 2.0 + apd_graph->rangeMin()), w), 1, 0, AlignVCenter | AlignRight);
    tmp_l->setFont(f);

    tmp_lo->addWidget(tmp_l = new QLabel(QString().setNum(apd_graph->rangeMin()), w), 2, 0, AlignBottom | AlignRight);
    tmp_l->setFont(f);
  }


  if (!onlyNull || !delta_pi_graph_labels) { 

    /* Stim Graph Axis Labels */

    w = delta_pi_graph_labels = new QWidget(graphs); // dummy widget to have nested grids
    graphlayout->addWidget(w, 3, 0);

    tmp_lo = new QGridLayout(w, 3, 1);

    tmp_lo->addWidget(tmp_l = new QLabel(QString().setNum(delta_pi_graph->rangeMax()), w), 0, 0, AlignTop | AlignRight); 
    tmp_l->setFont(f);

    tmp_lo->addWidget(tmp_l = new QLabel(QString().setNum((delta_pi_graph->rangeMax() - delta_pi_graph->rangeMin()) / 2.0 + delta_pi_graph->rangeMin()), w), 1, 0, AlignVCenter | AlignRight);
    tmp_l->setFont(f);

    tmp_lo->addWidget(tmp_l = new QLabel(QString().setNum(delta_pi_graph->rangeMin()), w), 2, 0, AlignBottom | AlignRight);
    tmp_l->setFont(f);

  }

  if (!onlyNull || !g_graph_labels) {

    /* G Graph Axis Labels */

    w = g_graph_labels = new QWidget(graphs); // dummy widget to have nested grids
    graphlayout->addWidget(w, 5, 0);

    tmp_lo = new QGridLayout(w, 3, 1);

    tmp_lo->addWidget(tmp_l = new QLabel(QString().setNum(g_graph->rangeMax()), w), 0, 0, AlignTop | AlignRight); 
    tmp_l->setFont(f);

    tmp_lo->addWidget(tmp_l = new QLabel(QString().setNum((g_graph->rangeMax() - g_graph->rangeMin()) / 2.0 + g_graph->rangeMin()), w), 1, 0, AlignVCenter | AlignRight);
    tmp_l->setFont(f);

    tmp_lo->addWidget(tmp_l = new QLabel(QString().setNum(g_graph->rangeMin()), w), 2, 0, AlignBottom | AlignRight);
    tmp_l->setFont(f);

  }

}

void APDcontrol::periodic()
{
  /* check the in fifo for data, and act upon it if found... */
  readInFifo();

  /* synch the ao_channels listbox with what is really in the kernel */
  synchAOChan();    

}

// read fifo from real-time-process, store in snapshot, and display via setText
void APDcontrol::readInFifo()
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
    apd_graph->plot(static_cast<double>(buf[i].apd));
    delta_pi_graph->plot(static_cast<double>(buf[i].delta_pi));
    g_graph->plot(buf[i].g_val);
    spooler->spool(buf, n_bufs);
  }
  /* now update the stats once per read() call (meaning we take the
     last MCSnapShot we got */
  if (!n_bufs) return;
  
  MCSnapShot & snapshot = buf[n_bufs-1];

  gui_indicator_pi->setText(QString::number(snapshot.pi));
  gui_indicator_delta_pi->setText(QString::number(snapshot.delta_pi));
  gui_indicator_apd->setText(QString::number(snapshot.apd));
  gui_indicator_di->setText(QString::number(snapshot.di));
  gui_indicator_v_apa->setText(QString::number(snapshot.v_apa));
  gui_indicator_v_baseline->setText(QString::number(snapshot.v_baseline));
  gui_indicator_ap_ti->setText(QString(uint64_to_cstr(snapshot.ap_ti)));
  gui_indicator_ap_tf->setText(QString(uint64_to_cstr(snapshot.ap_tf)));
  gui_indicator_g_val->setText(QString::number(snapshot.g_val));
  gui_indicator_delta_g->setText(QString::number(snapshot.delta_g));
  gui_indicator_consec_alternating->setText(QString::number(snapshot.consec_alternating));
  /* update our slider value representation if actual last read delta_g
     disagrees with the UI's notion of what it is... */
  static const float smallf = 0.0001;
#define f_equal(x, y) ( x > y ? x - y < smallf : y - x < smallf )
  if (!f_equal(snapshot.delta_g, gui_indicator_delta_g_bar_value->text().toFloat())) {
    disconnect(delta_g_bar, SIGNAL(valueChanged(int)), this, SLOT(changeDG(int)));
    delta_g_bar->setValue(mc_delta_g_toint(snapshot.delta_g));
    gui_indicator_delta_g_bar_value->setText(QString::number(snapshot.delta_g, 'g', 3));
    connect(delta_g_bar, SIGNAL(valueChanged(int)), this, SLOT(changeDG(int)));
  }

  /* incomplete... please finish! - Calin */
#undef f_equal
}

/* Possible race conditions here but it's too much trouble to implement
   atomicity for the free channels list right now... */
void APDcontrol::populateAOComboBox()
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
void APDcontrol::changeAOChan(const QString & selected)
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
void APDcontrol::synchAOChan() 
{
  QString ao_chan;

  ao_chan.setNum(shm->ao_chan);

  if (ao_channels->currentText() != ao_chan) 
    ao_channels->setSelected(ao_channels->findItem(ao_chan, CaseSensitive|ExactMatch), true); 
}

void APDcontrol::changeAIChan(const QString &chan)
{
  bool ok;

  shm->apd_channel = chan.toInt(&ok);

  if (!ok) shm->apd_channel = -1;
}

void APDcontrol::synchAIChan()
{
  uint i;
  int sel;

  disconnect(ai_channels, SIGNAL(activated(const QString &)), this, SLOT(changeAIChan(const QString &)));

 ai_channels->clear();
  ai_channels->insertItem("Off");
  for (i = 0; i < daq_shmCtl->numChannels(ComediSubDevice::AnalogInput); i++)
    if (daq_shmCtl->isChanOn(ComediSubDevice::AnalogInput, i))
      ai_channels->insertItem(QString().setNum(i));
  
  sel = ai_channels->findItem(QString().setNum(shm->apd_channel), CaseSensitive | ExactMatch);
  if (sel < 0) {
    ai_channels->setSelected(0);
    shm->apd_channel = -1;
  }
  else ai_channels->setSelected(sel); 

  connect(ai_channels, SIGNAL(activated(const QString &)), this, SLOT(changeAIChan(const QString &)));

}


void APDcontrol::changeAPDxx(const QString &xx_value)
{  
  int new_apd_xx;
  bool ok;

  new_apd_xx = xx_value.toInt(&ok);
  if (ok)
    shm->apd_xx = 1.0 - (0.01*new_apd_xx);
}

void APDcontrol::togglePacing(bool on)
{
  if (!on) {
    shm->pacing_on = 0;
  } else {
    shm->pacing_on = 1;
  }
}

void APDcontrol::changeNominalPI(int pi_in_ms)
{
  shm->nominal_pi = pi_in_ms;
}

void APDcontrol::toggleControl(bool on)
{
  
  if (!on) {
    shm->control_on = 0;
    g_adj_manual_only->setDisabled(true);
    gval->setDisabled(true);
    shm->g_adjustment_mode = MC_G_ADJ_MANUAL;
  } else {
    shm->control_on = 1;
    g_adj_manual_only->setDisabled(false);
    gval->setDisabled(!g_adj_manual_only->isChecked());
    shm->g_adjustment_mode = (g_adj_manual_only->isChecked() 
                              ? MC_G_ADJ_MANUAL 
                              : MC_G_ADJ_AUTOMATIC);
  }
}

void APDcontrol::toggleUnderlying(bool on)
{
  if (!on) {
    shm->continue_underlying = 0;
  } else {
    shm->continue_underlying = 1;
  }
}

void APDcontrol::toggleOnlyNegativePerturbations(bool on)
{
  if (!on) {
    shm->only_negative_perturbations = 0;
  } else {
    shm->only_negative_perturbations = 1;
  }
}

void APDcontrol::toggleTargetShorter(bool on)
{
  if (!on) {
    shm->target_shorter = 0;
  } else {
    shm->target_shorter = 1;
  }
}

void APDcontrol::gAdjManualOnly(int yes)
{
  shm->g_adjustment_mode = (yes ? MC_G_ADJ_MANUAL : MC_G_ADJ_AUTOMATIC);
}

void APDcontrol::changeG(const QString &g)
{  
  double newG;
  bool ok;

  newG = g.toDouble(&ok);
  if (ok)
    shm->g_val = newG;
}

void APDcontrol::changeDG(int new_dg_sliderval) 
{
  shm->delta_g = mc_delta_g_fromint(new_dg_sliderval);
  gui_indicator_delta_g_bar_value->setNum(mc_delta_g_fromint(new_dg_sliderval));
}

const char * APDcontrol::fileheader = 
"#File Format Version: " RCS_VERSION_STRING "\n"
"#--\n#Columns: \n"
"#scan_index, pacing_on, control_on, nominal_PI, PI, delta_PI, APD_xx, APD, previous_APD, DI, previous_DI, AP_ti, AP_tf, V_Apa, V_Baseline, g, delta_g, g_adjustment_mode, consecutive_alternating, only_negative_perturbations, continue_underlying, target_shorter\n";

//save important snapshot values to disk 
//important to edit this to suit your data needs
struct PVSaver
{  
  PVSaver (QTextStream & o, QString header = "") 
    : out(o), header(header) { out << header; };
  
  void operator()(MCSnapShot & ws) 
  { 
    QString scanIndex (uint64_to_cstr(ws.scan_index)),
            apTi      (uint64_to_cstr(ws.ap_ti)),
            apTf     (uint64_to_cstr(ws.ap_tf));
    
    dummy.sprintf
      ("%s %d %d %d %d %d %d %d %d %d %d %s %s %g %g %g %g %d %d %d %d %d\n",     
       scanIndex.latin1(), 
       (int)ws.pacing_on, (int)ws.control_on,
       ws.nominal_pi, ws.pi, ws.delta_pi, 
       ws.apd_xx, ws.apd, ws.previous_apd, ws.di, ws.previous_di, 
       apTi.latin1(), apTf.latin1(), ws.v_apa, ws.v_baseline, 
       ws.g_val, ws.delta_g, (int)ws.g_adjustment_mode, ws.consec_alternating, 
       (int)ws.only_negative_perturbations, (int)ws.continue_underlying, (int)ws.target_shorter);

    out << dummy;
  }

  QTextStream &out;
  QString dummy, header;
};

void APDcontrol::save()
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

void APDcontrol::saveAs()
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

void APDcontrol::safelyQuit()
{
  if (need_to_save) {
    int usersaid = 
      QMessageBox::warning(window, "Data is unsaved", "The APD Control Plugin is terminating, and the data for the APD Control Control Experiment is unsaved.\nDo you wish to save or discard changes?", "Save Changes", "Discard Changes");
    if (usersaid == 0)
      save(); /* this way even on file pick error
				     they will be prompted to save */
  }
}


void APDcontrol::buildRangeComboBoxesAndConnectSignals()
{
  static const GraphRangeSettings *arrays[] 
    = { apd_ranges, delta_pi_ranges, g_ranges, 0 };
  static const int sizes[] = { n_apd_ranges, n_delta_pi_ranges, n_g_ranges };
  QComboBox *boxes[] = { apd_range_ctl, delta_pi_range_ctl, g_range_ctl };

  QComboBox **box;
  const GraphRangeSettings **array;
  const int *size;
  int i;

  for ( box = boxes, size = sizes, array = arrays; 
        *array; 
        array++, size++, box++ ) 
    {
      for (i = 0; i < *size; i++) {
        const GraphRangeSettings & s = (*array)[i];
        (*box)->insertItem(QString().setNum(s.min) + " - " 
                           + QString().setNum(s.max), i);
      }
      (*box)->setCurrentItem(0);
      connect(*box, SIGNAL(activated(int)), 
              this, SLOT(graphHasChangedRange(int)));
    } 
}


void APDcontrol::graphHasChangedRange(int index)
{
  static const GraphRangeSettings *arrays[] 
    = { apd_ranges, delta_pi_ranges, g_ranges };
  ECGGraph * graphs[] = { apd_graph, delta_pi_graph, g_graph };
  const QComboBox *boxes[] = { apd_range_ctl, delta_pi_range_ctl, g_range_ctl, 0};
  QWidget **labelWidgets[] = { &apd_graph_labels, &delta_pi_graph_labels,
                               &g_graph_labels };
  
  QWidget ***w;
  ECGGraph **g;
  const QComboBox **box;
  const QObject *s = sender();

  for (g = graphs, box = boxes, w = labelWidgets; *box; g++, box++, w++) {
    if (s == *box) {
      const GraphRangeSettings & setting = (arrays[g - graphs])[index];
      (*g)->setRange(setting.min, setting.max);
      /* lazy man's rebuilding of all the labels -- slow, but fast enough */
      delete **w; **w = 0;

      /* now that the particular label is deleted, 
         re-create it using addAxisLabels(false)  then show() them.. */
      addAxisLabels(true);

      (**w)->show();

      break;
    }
  }
}


#undef spooler
