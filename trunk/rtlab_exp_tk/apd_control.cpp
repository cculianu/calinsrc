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
#include <qpen.h>
#include <qcolor.h>

#include <set>

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
              "addon was developed for cardiac action-potential-duration control "
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

/* This is the color of every other action potential the APD graphs */
const QColor APColor1 = "#ff0000", APColor2 = "#cccccc";
const unsigned int apdGraphPointWidth = 4; // only really affects the 'all apds' graph


static const QRegExp ELECTRODE_ORDER_RE("^\\s*\\d+\\s*(,\\s*\\d+\\s*)*$");

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

  //masterlayout = new QGridLayout(window, 2, 2);
  masterlayout = new QGridLayout(window, 3, 2);

  QMenuBar *mb = new QMenuBar(window);
  masterlayout->addMultiCellWidget(mb, 0, 0, 0, 1);

  QPopupMenu *fileMenu = new QPopupMenu(mb, "File");
  mb->insertItem("&File", fileMenu);
  fileMenu->insertItem("&Save", this, SLOT ( save() ) );
  fileMenu->insertItem("Save&As", this, SLOT ( saveAs() ) );

  /* graphs.. */
  graphs = new QWidget(window, "APD Control Graph Virtual Widget");
  //masterlayout->addWidget(graphs, 1, 1);
  masterlayout->addWidget(graphs, 2, 0);
  masterlayout->setColStretch(1, 2);

  int num_graph_columns = 3; //2 columns of graphs, plus one spacer in between
  int num_graph_rows = NumAPDGraphs+2; //NumAPDGraphs/2 rows of graphs, plus spacers
                                                                        //plus 2 more for the graph showing all electrodes
  graphlayout = new QGridLayout(graphs, num_graph_rows, num_graph_columns);

  graphlayout->setColStretch(0, 20); //graphs are 20X as wide as space between
  graphlayout->setColStretch(1, 1);
  graphlayout->setColStretch(2, 20);

  for (int i=0; i<num_graph_rows; i++)
    if (!(i%2)) graphlayout->setRowStretch(i, 10); //graphs are 10X as high as spaces between
    else graphlayout->setRowStretch(i, 1);

  // create the APD Monitor class that monitors beat numbers
  apd_monitor = new APDMonitor(APColor1, APColor2, this);

  //next draw the individual apd_graphs

  int 
    which_graph_row,
    which_graph_column,
    which_apd_graph;
  for (which_apd_graph=0; which_apd_graph<(NumAPDGraphs+1); which_apd_graph++) {
    apd_graph[which_apd_graph]   = new ECGGraph (beats_per_gridline, num_gridlines, 
                                                 apd_ranges[0].min, apd_ranges[0].max, 
                                                 graphs, QString(name()) + " - APD");
    apd_graph[which_apd_graph]->setBlockFactor(0);
    apd_graph[which_apd_graph]->setPlotMode(ECGGraph::Circles);
    apd_graph[which_apd_graph]->setPointSize(apdGraphPointWidth);
    apd_graph[which_apd_graph]->setBlipSize((int)(apdGraphPointWidth*1.5));
    determineGraphRowColumn(which_apd_graph,which_graph_row,which_graph_column);

    graphlayout->addWidget(apd_graph[which_apd_graph], which_graph_row,which_graph_column,0);
    if (which_apd_graph < NumAPDGraphs)
      graphlayout->addWidget(new QLabel(QString("Electrode %1").arg(which_apd_graph), graphs), which_graph_row,which_graph_column,(Qt::AlignTop | Qt::AlignRight));
    else {
      // the 'All APD's' graph..
      graphlayout->addWidget(new QLabel(QString("All Used Electrodes"), graphs), which_graph_row,which_graph_column,(Qt::AlignTop | Qt::AlignRight));

      // create the interleaver class that talks to this graph
      apd_interleaver = new APDInterleaver(this, apd_graph[which_apd_graph],
                                           apd_monitor);

      apd_graph[which_apd_graph]->setPlotMode(ECGGraph::Circles);
      apd_graph[which_apd_graph]->plotPen().setWidth(apdGraphPointWidth);
    }
    apd_range_ctl[which_apd_graph]=new QComboBox(false, graphs, "APD Range Combo box");
    graphlayout->addWidget(apd_range_ctl[which_apd_graph],which_graph_row,which_graph_column,(Qt::AlignTop | Qt::AlignLeft));

    //addAxisLabels(which_apd_graph,which_graph_row,which_graph_column);
    buildRangeComboBoxesAndConnectSignals(which_apd_graph);
  }

  QVBox *vb = new QVBox(graphs);
  determineGraphRowColumn(which_apd_graph,which_graph_row,which_graph_column);
  graphlayout->addWidget(vb, which_graph_row, which_graph_column);
  QHBox *eoBox = new QHBox(vb);
  (void)new QLabel("Master Electrode Order: ", eoBox);
  QLineEdit *eo = new QLineEdit(apd_monitor->masterOrder(), eoBox);
  eoBox = new QHBox(vb);
  (void)new QLabel("Actual Order Being Used: ", eoBox);
  QLabel *eoLabel = new QLabel(apd_monitor->orderString(), eoBox);
  eo->setValidator(new QRegExpValidator(ELECTRODE_ORDER_RE, eo));
  connect(eo, SIGNAL(textChanged(const QString &)),
          apd_monitor, SLOT(setMasterOrder(const QString &)));
  connect(apd_monitor, SIGNAL(orderChanged(const QString &)),
          eoLabel, SLOT(setText(const QString &)));
  /* end graphs... */

/*******************************************************/
/* edit this section to change text displays etc. */
/*******************************************************/

  controls = new QWidget(window);
  masterlayout->addWidget(controls, 1, 0);

  const int n_ctl_r = 3, n_ctl_c = 1;
  controlslayout = new QGridLayout(controls, n_ctl_r, n_ctl_c);

  //SignalMappers to connect duplicated widgets to actions for
  //the appropriate signal/graph
  ai_channels_mapper = new QSignalMapper(this);
  nominal_pi_mapper = new QSignalMapper(this);
  gval_mapper = new QSignalMapper(this);
  //apd_xx_edit_box_mapper = new QSignalMapper(this);
  delta_g_bar_mapper = new QSignalMapper(this);
  pacing_toggle_mapper = new QSignalMapper(this);
  control_toggle_mapper = new QSignalMapper(this);
  underlying_toggle_mapper = new QSignalMapper(this);
  only_negative_toggle_mapper = new QSignalMapper(this);
  target_shorter_toggle_mapper = new QSignalMapper(this);
  g_adj_manual_only_mapper = new QSignalMapper(this);

  { /* ctls_ao0 and ctls_ao1 groupbox */    //**** ask calin what the point of this { is ???
    // Dave -- I wanted to logically group the below -- no real point

    /*******************************************************/
    /* edit this section to change GUI controls      
       note that to change the functionality of a 
       particular control, you have to change its
       slot function directly                                        */
    /*******************************************************/

    for (int which_ao_chan=0; which_ao_chan<NumAOchannels; which_ao_chan++) {

    QGroupBox *ctls_ao = new QGroupBox(1, Vertical, QString("AO%1 Controls").arg(which_ao_chan), controls);
    controlslayout->addMultiCellWidget(ctls_ao, which_ao_chan, which_ao_chan, 0, n_ctl_c-1);

    QWidget *tmpw = new QWidget(ctls_ao);
    QGridLayout *tmplo = new QGridLayout(tmpw, 11, 4);

    // text displays
    int line_counter=0;
    tmplo->addWidget(new QLabel("APD_n (ms): ", tmpw), line_counter++, 0);
    tmplo->addWidget(new QLabel("DI_n: ", tmpw), line_counter++, 0);
    tmplo->addWidget(new QLabel("PI = APD+DI(ms): ", tmpw), line_counter++, 0);
    line_counter=0;
    tmplo->addWidget(new QLabel("delta PI (ms): ", tmpw), line_counter++, 2);
    tmplo->addWidget(new QLabel("g: ", tmpw), line_counter++, 2);
    tmplo->addWidget(new QLabel("delta g: ", tmpw), line_counter++, 2);
    
    line_counter=0;
    tmplo->addWidget(gui_indicator_apd[which_ao_chan] = new QLabel(tmpw), line_counter++, 1); 
    tmplo->addWidget(gui_indicator_di[which_ao_chan] = new QLabel(tmpw), line_counter++, 1);    
    tmplo->addWidget(gui_indicator_pi[which_ao_chan] = new QLabel(tmpw), line_counter++, 1);
    line_counter=0;
    tmplo->addWidget(gui_indicator_delta_pi[which_ao_chan] = new QLabel(tmpw), line_counter++, 3);
    tmplo->addWidget(gui_indicator_g_val[which_ao_chan] = new QLabel(tmpw), line_counter++, 3);
    tmplo->addWidget(gui_indicator_delta_g[which_ao_chan] = new QLabel(tmpw), line_counter++, 3);

    QVBox *tmpvb = new QVBox(ctls_ao);
    QHBox *tmphb = new QHBox(tmpvb);
    tmphb->setSpacing(5);

    (void)new QLabel("AI channel to measure APDs:", tmphb);

    ai_channels[which_ao_chan] = new SearchableComboBox(tmphb);
    if (which_ao_chan==(NumAOchannels-1)) { // only run these the first time through the loop
      synchAIChan();
      connect(daqSystem(), SIGNAL(channelOpened(uint)), this, SLOT(synchAIChan()));
      connect(daqSystem(), SIGNAL(channelClosed(uint)), this, SLOT(synchAIChan()));
    }
    tmphb->setStretchFactor(new QWidget(tmphb), 1);     /* Invisible spacer widget    |---------| */
    ai_channels_mapper->setMapping(ai_channels[which_ao_chan],which_ao_chan);
    connect (ai_channels[which_ao_chan], SIGNAL(activated(const QString &)),
	     ai_channels_mapper, SLOT(map()));
    connect(ai_channels_mapper, SIGNAL(mapped(int)),
	    this, SLOT(changeAIChan(int)));

    if (!which_ao_chan) {  //we only want one of these (for AO0)
      (void) new QLabel("APD90,80,70,...:", tmphb);
      apd_xx_edit_box = new QLineEdit(tmphb);
      apd_xx_edit_box->setValidator(new QIntValidator(apd_xx_edit_box));
      apd_xx_edit_box->setText(QString("%1").arg( (int)(100*(1.0-(shm->apd_xx)))) );
      apd_xx_edit_box->setMaxLength(3);
      connect(apd_xx_edit_box, SIGNAL(textChanged(const QString &)), this, SLOT(changeAPDxx(const QString &)));
      apd_xx_edit_box->setMaximumWidth(apd_xx_edit_box->minimumSizeHint().width());
    }
    /* We only want one of these ... not mapper
    (void) new QLabel("APD90,80,70,...:", tmphb);
    apd_xx_edit_box[which_ao_chan] = new QLineEdit(tmphb);
    apd_xx_edit_box[which_ao_chan]->setValidator(new QIntValidator(apd_xx_edit_box[which_ao_chan]));
    apd_xx_edit_box[which_ao_chan]->setText(QString("%1").arg( (int)(100*(1.0-(shm->apd_xx)))) );
    apd_xx_edit_box[which_ao_chan]->setMaxLength(3);
    apd_xx_edit_box[which_ao_chan]->setMaximumWidth(apd_xx_edit_box[which_ao_chan]->minimumSizeHint().width());
    apd_xx_edit_box_mapper->setMapping(apd_xx_edit_box[which_ao_chan],which_ao_chan);
    connect (apd_xx_edit_box[which_ao_chan], SIGNAL(valueChanged(int)),
	     apd_xx_edit_box_mapper, SLOT(map()));
    connect(apd_xx_edit_box_mapper, SIGNAL(mapped(int)),
	    this, SLOT(changeAPDxx(int)));
    */

    //new line of control widgets
    tmphb = new QHBox(tmpvb);        
    tmphb->setSpacing(5);

    pacing_toggle[which_ao_chan] = new QCheckBox("Pacing ON", tmphb);
    pacing_toggle[which_ao_chan]->setChecked(shm->pacing_on[which_ao_chan]);    
    tmphb->setStretchFactor(new QWidget(tmphb), 1);     /* Invisible spacer widget    |---------| */
    pacing_toggle_mapper->setMapping(pacing_toggle[which_ao_chan],which_ao_chan);
    connect (pacing_toggle[which_ao_chan], SIGNAL(toggled(bool)),
	     pacing_toggle_mapper, SLOT(map()));
    connect(pacing_toggle_mapper, SIGNAL(mapped(int)),
	    this, SLOT(togglePacing(int)));
    
    (void) new QLabel("Nominal PI:", tmphb);
    nominal_pi[which_ao_chan] = new QSpinBox(1, 2000, 1, tmphb);    
    nominal_pi[which_ao_chan]->setValue(static_cast<int>(shm->nominal_pi[which_ao_chan]));
    nominal_pi_mapper->setMapping(nominal_pi[which_ao_chan],which_ao_chan);
    connect (nominal_pi[which_ao_chan], SIGNAL(valueChanged(int)),
	     nominal_pi_mapper, SLOT(map()));
    connect(nominal_pi_mapper, SIGNAL(mapped(int)),
	    this, SLOT(changeNominalPI(int)));

    if (which_ao_chan==1) {  //this is specific to AO1
      //new line of control widgets
      tmphb = new QHBox(tmpvb);
      tmphb->setSpacing(5);

      link_to_ao0_toggle = new QCheckBox("Link control to AO0 pacing", tmphb);
      link_to_ao0_toggle->setChecked(shm->link_to_ao0);    
      tmphb->setStretchFactor(new QWidget(tmphb), 1);     /* Invisible spacer widget    |---------| */
      connect(link_to_ao0_toggle, SIGNAL(toggled(bool)), this, SLOT(toggleLinkToAO0(bool)));

      (void) new QLabel("AO0 to AO1 conduction (ms):", tmphb);
      ao0_ao1_cond_time = new QSpinBox(1, 20, 1, tmphb);    
      shm->ao0_ao1_cond_time = 5; //assume 5 ms, based on 1cm btwn sites & 2 m/s conduction
      ao0_ao1_cond_time->setValue(static_cast<int>(shm->ao0_ao1_cond_time));
      connect(ao0_ao1_cond_time, SIGNAL(valueChanged(int)), this, SLOT(changeAO0_AO1_CondTime(int)));
    }

    //new line of control widgets
    tmphb = new QHBox(tmpvb);
    tmphb->setSpacing(5);

    control_toggle[which_ao_chan] = new QCheckBox("Control ON", tmphb);
    control_toggle[which_ao_chan]->setChecked(shm->control_on[which_ao_chan]);    
    control_toggle_mapper->setMapping(control_toggle[which_ao_chan],which_ao_chan);
    connect (control_toggle[which_ao_chan], SIGNAL(toggled(bool)),
	     control_toggle_mapper, SLOT(map()));
    connect(control_toggle_mapper, SIGNAL(mapped(int)),
	    this, SLOT(toggleControl(int)));

    underlying_toggle[which_ao_chan] = new QCheckBox("Continue underlying pacing", tmphb);
    underlying_toggle[which_ao_chan]->setChecked(shm->continue_underlying[which_ao_chan]);
    underlying_toggle_mapper->setMapping(underlying_toggle[which_ao_chan],which_ao_chan);
    connect (underlying_toggle[which_ao_chan], SIGNAL(toggled(bool)),
	     underlying_toggle_mapper, SLOT(map()));
    connect(underlying_toggle_mapper, SIGNAL(mapped(int)),
	    this, SLOT(toggleUnderlying(int)));

    //new line of control widgets
    tmphb = new QHBox(tmpvb);
    tmphb->setSpacing(5);

    only_negative_toggle[which_ao_chan] = new QCheckBox("Only negative perturbations", tmphb);
    only_negative_toggle[which_ao_chan]->setChecked(shm->only_negative_perturbations[which_ao_chan]);    
    only_negative_toggle_mapper->setMapping(only_negative_toggle[which_ao_chan],which_ao_chan);
    connect (only_negative_toggle[which_ao_chan], SIGNAL(toggled(bool)),
	     only_negative_toggle_mapper, SLOT(map()));
    connect(only_negative_toggle_mapper, SIGNAL(mapped(int)),
	    this, SLOT(toggleOnlyNegativePerturbations(int)));

    target_shorter_toggle[which_ao_chan] = new QCheckBox("Target shorter APD initially", tmphb);
    target_shorter_toggle[which_ao_chan]->setChecked(shm->target_shorter[which_ao_chan]);    
    target_shorter_toggle_mapper->setMapping(target_shorter_toggle[which_ao_chan],which_ao_chan);
    connect (target_shorter_toggle[which_ao_chan], SIGNAL(toggled(bool)),
	     target_shorter_toggle_mapper, SLOT(map()));
    connect(target_shorter_toggle_mapper, SIGNAL(mapped(int)),
	    this, SLOT(toggleTargetShorter(int)));

    //new line of control widgets
    tmphb = new QHBox(tmpvb);
    tmphb->setSpacing(5);

    (void) new QLabel("g: ", tmphb);
    gval[which_ao_chan] = new QLineEdit(tmphb);
    gval[which_ao_chan]->setValidator(new QDoubleValidator(gval[which_ao_chan]));
    gval[which_ao_chan]->setText(QString("%1").arg(shm->g_val[which_ao_chan]));
    gval_mapper->setMapping(gval[which_ao_chan],which_ao_chan);
    connect (gval[which_ao_chan], SIGNAL(textChanged(const QString &)),
	     gval_mapper, SLOT(map()));
    connect(gval_mapper, SIGNAL(mapped(int)),
	    this, SLOT(changeG(int)));

    g_adj_manual_only[which_ao_chan] = new QCheckBox("Adjust g Only Manually", tmphb);
    g_adj_manual_only[which_ao_chan]->setChecked(shm->g_adjustment_mode[which_ao_chan] == MC_G_ADJ_MANUAL);
    g_adj_manual_only[which_ao_chan]->setDisabled(!shm->control_on[which_ao_chan]);
    g_adj_manual_only_mapper->setMapping(g_adj_manual_only[which_ao_chan],which_ao_chan);
    connect (g_adj_manual_only[which_ao_chan], SIGNAL(toggled(bool)),
	     g_adj_manual_only_mapper, SLOT(map()));
    connect(g_adj_manual_only_mapper, SIGNAL(mapped(int)),
	    this, SLOT(gAdjManualOnly(int)));

    (void)new QLabel("delta g:", tmphb);
    /* delta-g controls */   
    gui_indicator_delta_g_bar_value[which_ao_chan] = new QLabel(stringifyDeltaG(shm->delta_g[which_ao_chan]), tmphb);
    delta_g_bar[which_ao_chan] = new QScrollBar
      ( mc_delta_g_toint(MC_DELTA_G_MIN), mc_delta_g_toint(MC_DELTA_G_MAX),
        MC_DELTA_G_SCROLLBAR_STEP, MC_DELTA_G_SCROLLBAR_STEP * 5, 
        mc_delta_g_toint(shm->delta_g[which_ao_chan]), Qt::Horizontal, tmphb );
     delta_g_bar[which_ao_chan]->setSizePolicy(QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed)); 
     delta_g_bar[which_ao_chan]->setMinimumSize(100, delta_g_bar[which_ao_chan]->minimumSize().height()); // force this scrollbar to EXIST!
     tmphb->setStretchFactor(delta_g_bar[which_ao_chan], 3);
    delta_g_bar_mapper->setMapping(delta_g_bar[which_ao_chan],which_ao_chan);
    connect (delta_g_bar[which_ao_chan], SIGNAL(valueChanged(int)),
	     delta_g_bar_mapper, SLOT(map()));
    connect(delta_g_bar_mapper, SIGNAL(mapped(int)),
	    this, SLOT(changeDG(int)));

    } //end of for (which_ao_chan ...
  
  controlslayout->addMultiCellWidget // footnote at the bottom
    (new QLabel(QString("APD control experiment notes:\n"
                        "x-axis displays %1 points (%2 points per vertical gridline).")
               .arg(beats_per_gridline * num_gridlines)
               .arg(beats_per_gridline), controls), 
     n_ctl_r-1, n_ctl_r-1, 0, n_ctl_c-1, AlignBottom | AlignLeft);

  // when a new channel is opened, we want to synch that channel's graph
  // with teh other graphs.. the below slot does this
  connect(apd_monitor, SIGNAL(orderChanged(const QString &)),
          this, SLOT(ffwdAllGraphs()));

  window_id = daqSystem()->windowMenuAddWindow(window);  
  window->show();

  timer = new QTimer(this);
  connect(timer, SIGNAL(timeout()), this, SLOT(periodic()));
  timer->start(50);
  }
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
  int i;

  timer->stop();
  safelyQuit(); /* prompts the user to save, if necessary.. kinda sloppy to 
                   put this in a destructor.. but 'oh well' */
  daqSystem()->windowMenuRemoveWindow(window_id);

  if (shm) for (i=0; i<NumAOchannels; i++) shm->pacing_on[i] = 0; // to 'turn off' pacing ...

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
void APDcontrol::addAxisLabels(int which_apd_graph, int which_graph_row, 
                               int which_graph_column)
{
  /* add the axis labels */
  QGridLayout *tmp_lo;
  QLabel *tmp_l; // temporary pointer to axis labels
  QFont f; f.setPointSize(8); // label point size


  QWidget *w; /* tmp working wiget pointer points to members:
                 apd_graph_labels, delta_pi_graph_labels, etc.. */

  if (!apd_graph_labels[which_apd_graph]) {

    /* APD Graph Axis Labels */

    w = apd_graph_labels[which_apd_graph] = new QWidget(graphs); // dummy widget to have nested grids

    graphlayout->addWidget(w, 1, 0);
    //the following doesn't seem to work, so I went back to (w,1,0), b/c at least that puts one
    //set of axis labels on the screen, so at least it's something to work with for future
    //modifications when I, or someone, gets a chance. It'll need to be based on 
    //which_graph_row, which_graph_column
    // graphlayout->addWidget(w, which_graph_row, which_graph_column);
    
    tmp_lo = new QGridLayout(w, 3, 1);

    tmp_lo->addWidget(tmp_l = new QLabel(QString().setNum(apd_graph[which_apd_graph]->rangeMax()), w), 0, 0, AlignTop | AlignRight); 
    tmp_l->setFont(f);

    tmp_lo->addWidget(tmp_l = new QLabel(QString().setNum((apd_graph[which_apd_graph]->rangeMax() - apd_graph[which_apd_graph]->rangeMin()) / 2.0 + apd_graph[which_apd_graph]->rangeMin()), w), 1, 0, AlignVCenter | AlignRight);
    tmp_l->setFont(f);

    tmp_lo->addWidget(tmp_l = new QLabel(QString().setNum(apd_graph[which_apd_graph]->rangeMin()), w), 2, 0, AlignBottom | AlignRight);
    tmp_l->setFont(f);
  }

}

void APDcontrol::determineGraphRowColumn(int apd_graph, int &graph_row, 
                                        int &graph_column) {
    graph_row = 2*apd_graph;
    graph_column = 0;
    if (apd_graph > ((NumAPDGraphs-1) / 2) ) {
      graph_row -= NumAPDGraphs;
      graph_column = 2;
    }
    if (apd_graph >= NumAPDGraphs) {
      graph_row = (apd_graph/2)*2;
      graph_column = 2*(apd_graph % 2);
    }
}

void APDcontrol::periodic()
{
  /* check the in fifo for data, and act upon it if found... */
  readInFifo();
}

void APDcontrol::ffwdGraph(uint chan)
{
  if (chan > NumAPDGraphs) return;

  bool found;
  uint order;
  order = apd_monitor->orderOf(chan, &found);
  if (found) {
    apd_graph[apd_monitor->orderOf(chan)]->reset();
    apd_graph[apd_monitor->orderOf(chan)]->ffwd(apd_monitor->beatNumber());
  }
}

void APDcontrol::ffwdAllGraphs()
{
  uint i;
  for (i = 0; i < NumAPDGraphs; i++) ffwdGraph(i);
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

  spooler->spool(buf, n_bufs);

  need_to_save = (n_bufs ? true : need_to_save);
  
  //initialize
  int ao_chan_buf[NumAOchannels];
  for (i=0; i<NumAOchannels; i++) ao_chan_buf[i]=-1;

  /*  plot to graphs here... */
  for (i = 0; i < n_bufs; i++) {
    /*
      if (buf[i].ao_chan >= 0) {
       apd_graph[buf[i].ao_chan]->plot(static_cast<double>(buf[i].apd));
       ao_chan_buf[buf[i].ao_chan]=i;
      }
    */

    /* HACK to alternate colors every other AP 
       also monitors current beat count */
    apd_monitor->gotAPD(buf + i);
    uint chan = apd_monitor->orderOf(buf[i].apd_channel);

    apd_graph[chan]->plotPen().setColor(apd_monitor->currentColor());
    apd_graph[chan]->blipPen().setColor(apd_monitor->currentColor());

    apd_graph[chan]->plot(static_cast<double>(buf[i].apd));

    // this is responsible for drawing apd's to the last graph in the 
    // perkinje-fiber-friendly layout
    apd_interleaver->gotAPD(buf + i);

    if (buf[i].ao_chan >= 0) ao_chan_buf[buf[i].ao_chan]=i; //for chans with associated ao_chan

    /*
      Dave's early-stage attempt at plotting all apds ... jettisoned in favor of Calin's class
    all_apd_graph[buf[i].apd_channel] = buf[i].apd;
    num_apds_in_all_apds++;

    if (!(num_apds_in_all_apds%NUM_THRESHS_DRAWN)) {
          if (!(num_apds_in_all_apds%(2*NUM_THRESHS_DRAWN))) {
	    DumpAllAPDSwithonecolor();
	    num_apds_in_all_apds=0; //reset after 2 draws
	  }      
	  else
	    DumpAllAPDSwithOtherColor;
    }

    */
}

  /* now update the stats once per read() call (meaning we take the
     latest MCSnapShot that corresponds to AO0 and AO1 */
  for (i=0; i<NumAOchannels; i++) {
    if (ao_chan_buf[i]>=0) { //if it = -1, then none of the buffers contained data for
                                                        //this AO channel
  
      MCSnapShot & snapshot = buf[ao_chan_buf[i]];

      gui_indicator_pi[i]->setText(QString::number(snapshot.pi));
      gui_indicator_delta_pi[i]->setText(QString::number(snapshot.delta_pi));
      gui_indicator_apd[i]->setText(QString::number(snapshot.apd));
      gui_indicator_di[i]->setText(QString::number(snapshot.di));
      gui_indicator_g_val[i]->setText(QString::number(snapshot.g_val));
      gui_indicator_delta_g[i]->setText(stringifyDeltaG(snapshot.delta_g));
      if (gui_indicator_g_val[i]->text() !=  gui_indicator_delta_g_bar_value[i]->text()) {
	disconnect (delta_g_bar[i], SIGNAL(valueChanged(int)),
		 delta_g_bar_mapper, SLOT(map()));
	delta_g_bar[i]->setValue(mc_delta_g_toint(snapshot.delta_g));
	gui_indicator_delta_g_bar_value[i]->setText(stringifyDeltaG(snapshot.delta_g));
	connect (delta_g_bar[i], SIGNAL(valueChanged(int)),
		 delta_g_bar_mapper, SLOT(map()));
      }
    }
  }
}

void APDcontrol::changeAIChan(int which_ao_chan)
{
  QString chan;
  bool ok;

  chan = ai_channels[which_ao_chan]->currentText ();
  shm->ai_chan_this_ao_chan_is_dependent_on[which_ao_chan] = chan.toInt(&ok);

  if (!ok) shm->ai_chan_this_ao_chan_is_dependent_on[which_ao_chan] = -1;
}

void APDcontrol::synchAIChan()
{
  uint i;
  int sel;

  for (int which_ao_chan=0; which_ao_chan<NumAOchannels; which_ao_chan++) {

    SearchableComboBox *aic = ai_channels[which_ao_chan];
    disconnect (aic, SIGNAL(activated(const QString &)),
	     ai_channels_mapper, SLOT(map()));
    //disconnect(aic, SIGNAL(activated(const QString &)), this, SLOT(changeAIChan(const QString &)));
 
    aic->clear();
    aic->insertItem("Off");
    for (i = 0; i < daq_shmCtl->numChannels(ComediSubDevice::AnalogInput); i++)
      if (daq_shmCtl->isChanOn(ComediSubDevice::AnalogInput, i))
	aic->insertItem(QString().setNum(i));
  
    sel = aic->findItem(QString().setNum(shm->ai_chan_this_ao_chan_is_dependent_on[which_ao_chan]), CaseSensitive | ExactMatch);
    if (sel < 0) {
      aic->setSelected(0);
      shm->ai_chan_this_ao_chan_is_dependent_on[which_ao_chan] = -1;
    }
    else aic->setSelected(sel); 

    connect (aic, SIGNAL(activated(const QString &)),
	     ai_channels_mapper, SLOT(map()));
    //connect(aic, SIGNAL(activated(const QString &)), this, SLOT(changeAIChan(const QString &)));
  }
}

void APDcontrol::changeAPDxx(const QString &xx_value)
{  
  int new_apd_xx;
  bool ok;

  new_apd_xx = xx_value.toInt(&ok);
  if (ok)
    shm->apd_xx = 1.0 - (0.01*new_apd_xx);
}

void APDcontrol::togglePacing(int which_ao_chan)
{
    shm->pacing_on[which_ao_chan] = pacing_toggle[which_ao_chan]->isChecked();
}

void APDcontrol::changeNominalPI(int which_ao_chan)
{
  shm->nominal_pi[which_ao_chan] = nominal_pi[which_ao_chan]->value();
}

void APDcontrol::toggleLinkToAO0(bool on)
{
  shm->link_to_ao0 = on;
}


void APDcontrol::changeAO0_AO1_CondTime(int cond_time)
{
  shm->ao0_ao1_cond_time = cond_time;
}

void APDcontrol::toggleControl(int which_ao_chan)
{
  
  if (!(control_toggle[which_ao_chan]->isChecked())) {
    shm->control_on[which_ao_chan] = 0;
    g_adj_manual_only[which_ao_chan]->setDisabled(true);
    gval[which_ao_chan]->setDisabled(true);
    shm->g_adjustment_mode[which_ao_chan] = MC_G_ADJ_MANUAL;
  } else {
    shm->control_on[which_ao_chan] = 1;
    g_adj_manual_only[which_ao_chan]->setDisabled(false);
    gval[which_ao_chan]->setDisabled(!g_adj_manual_only[which_ao_chan]->isChecked());
    shm->g_adjustment_mode[which_ao_chan] = 
      (g_adj_manual_only[which_ao_chan]->isChecked() 
                              ? MC_G_ADJ_MANUAL 
                              : MC_G_ADJ_AUTOMATIC);
  }
}

void APDcontrol::toggleUnderlying(int which_ao_chan)
{
  shm->continue_underlying[which_ao_chan] = underlying_toggle[which_ao_chan]->isChecked();
}

void APDcontrol::toggleOnlyNegativePerturbations(int which_ao_chan)
{
  shm->only_negative_perturbations[which_ao_chan] = only_negative_toggle[which_ao_chan]->isChecked();
}

void APDcontrol::toggleTargetShorter(int which_ao_chan)
{
  shm->target_shorter[which_ao_chan] = target_shorter_toggle[which_ao_chan]->isChecked();
}

void APDcontrol::gAdjManualOnly(int which_ao_chan)
{
  if (!(g_adj_manual_only[which_ao_chan]->isChecked())) {
    shm->g_adjustment_mode[which_ao_chan]= MC_G_ADJ_AUTOMATIC;
  } else {
    shm->g_adjustment_mode[which_ao_chan]= MC_G_ADJ_MANUAL;
  }
}

void APDcontrol::changeG(int which_ao_chan)
{  
  QString g;
  double newG;
  bool ok;

  g = gval[which_ao_chan]->text();
  newG = g.toDouble(&ok);
  if (ok)
    shm->g_val[which_ao_chan] = newG;
}

void APDcontrol::changeDG(int which_ao_chan)
{
  int new_dg_sliderval;
  new_dg_sliderval = delta_g_bar[which_ao_chan]->value();
  shm->delta_g[which_ao_chan] = mc_delta_g_fromint(new_dg_sliderval);
  gui_indicator_delta_g_bar_value[which_ao_chan]->setText(stringifyDeltaG(mc_delta_g_fromint(new_dg_sliderval)));
}

const char * APDcontrol::fileheader = 
"#File Format Version: " RCS_VERSION_STRING "\n"
"#--\n#Columns: \n"
"#APD_channel AO_channel scan_index, pacing_on, control_on, nominal_PI, PI, delta_PI, APD_xx, APD,  DI, AP_ti, AP_tf, V_Apa, V_Baseline, g, delta_g, g_adjustment_mode, consecutive_alternating, only_negative_perturbations, continue_underlying, target_shorter, link_to_ao0, ao0_ao1_conduction_time\n";

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
      ("%d %d %s %d %d %d %d %d %d %d %d %s %s %g %g %g %g %d %d %d %d %d %d %d\n",     
       ws.apd_channel, 
       ws.ao_chan, 
       scanIndex.latin1(), 
       (int)ws.pacing_on, 
       (int)ws.control_on,
       ws.nominal_pi, 
       ws.pi, 
       ws.delta_pi, 
       ws.apd_xx, 
       ws.apd, 
       ws.di, 
       apTi.latin1(), 
       apTf.latin1(), 
       ws.v_apa, 
       ws.v_baseline, 
       ws.g_val, 
       ws.delta_g, 
       (int)ws.g_adjustment_mode, 
       ws.consec_alternating, 
       (int)ws.only_negative_perturbations, 
       (int)ws.continue_underlying, 
       (int)ws.target_shorter,
       (int)ws.link_to_ao0,
       (int) ws.ao0_ao1_cond_time
);

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

void APDcontrol::buildRangeComboBoxesAndConnectSignals(int which_apd_graph)
{
  QComboBox *box = apd_range_ctl[which_apd_graph];
  const GraphRangeSettings * s = apd_ranges;
  const int size = n_apd_ranges;
  int i;

  for (i = 0; i < size; i++) 
    box->insertItem(QString("%1 - %2").arg(s[i].min).arg(s[i].max), i);
  
  box->setCurrentItem(0);

  QSignalMapper *mapper = new QSignalMapper(this);
  mapper->setMapping(box, which_apd_graph);
  connect(box, SIGNAL(activated(const QString &)), mapper, SLOT(map()));
  connect(mapper, SIGNAL(mapped(int)), 
	  this, SLOT(graphHasChangedRange(int)));
}


void APDcontrol::graphHasChangedRange(int which_apd_graph)
{
  int index = apd_range_ctl[which_apd_graph]->currentItem();

  int which_graph_row;
  int which_graph_column;
  determineGraphRowColumn(which_apd_graph,which_graph_row,which_graph_column);

  const GraphRangeSettings & setting = apd_ranges[index];
  apd_graph[which_apd_graph]->setRange(setting.min, setting.max);
  //delete apd_graph_labels[which_apd_graph];
  //apd_graph_labels[which_apd_graph] = 0;
  //addAxisLabels(which_apd_graph,which_graph_row,which_graph_column);
  //apd_graph_labels[which_apd_graph]->show();
}


/* Simply prints delta G to a string uses the constant
   MC_DELTA_G_GUI_PRECISION defined in apd_control.h to control the sprintf*/
QString APDcontrol::stringifyDeltaG(double g)
{
  return QString().sprintf("%.*f", MC_DELTA_G_GUI_PRECISION, g);
}

#undef spooler



/*----------------------------------------------------------------------------
  APD INTERLEAVER -- Don't touch this unless you are Calin and/or you 
  know what you are doing! 
-----------------------------------------------------------------------------*/


APDInterleaver::APDInterleaver(QObject *parent,
                               ECGGraph *g, 
                               const APDMonitor *monitor,
                               DAQSystem *_ds)
  : QObject(parent), graph(g), monitor(monitor)
{

  lastBeatGraphed = monitor->beatNumber();
  ds = _ds;
  if (!ds) {
    ds = *(DAQSystem::daqSystems().begin());
  }
  calculateChannels();
  connect(monitor, SIGNAL(orderChanged(const QString &)),
          this, SLOT(calculateChannels()));
  connect(monitor, SIGNAL(beatNumberChanged(uint)),
          this, SLOT(newBeat(uint)));
}

void APDInterleaver::calculateChannels()
{
  set<unsigned int> cs;
  set<unsigned int>::iterator it, end;

  channelAPDs.clear();
  cs = ds->channelsWithSpikeThresholds();
  for (it = cs.begin(), end = cs.end(); it != end; it++)  spikeSet(*it);
  reset();
  graph->setSecondsVisible(channelAPDs.size()+1);
}

void APDInterleaver::spikeSet(uint chan)
{
  (void)channelAPDs[chan]; /* doesn't matter if it's already there.. */
}

void APDInterleaver::reset()
{
  ChannelAPDs::iterator it, end;
  for (it = channelAPDs.begin(), end = channelAPDs.end(); it != end; it++) {
    APDPair & p = it->second;
    p.ct = 0;
    p.apd[0] = it->second.apd[1] = 0;
  }
}

void APDInterleaver::gotAPD(MCSnapShot *m)
{
  ChannelAPDs::iterator it;

  if ( (it = channelAPDs.find((uint)m->apd_channel)) == channelAPDs.end())
    return;
  
  APDPair & p = it->second;
  
  p.apd[p.ct % 2] = m->apd;
  p.color[p.ct % 2] = monitor->currentColor();
  p.ct++;
}

void APDInterleaver::newBeat(uint b)
{
  if (b-lastBeatGraphed > 2) {
    graphAPDs();
    reset();
  }
}

void APDInterleaver::graphAPDs()
{
  /* . <-- reset graph here.. */
  graph->reset();

  graph->ffwd(static_cast<uint>
              ((graph->secondsVisible()*graph->sampleRateHz())
               /(channelAPDs.size() + 1)
               - 2));

  ChannelAPDs::iterator 
    it = channelAPDs.begin(), 
    end = channelAPDs.end();
  vector<uint> order = monitor->orderVector();
  vector<uint>::iterator
    oit = order.begin(),
    oend = order.end();

  for (; oit != oend; oit++) {
    if ( (it = channelAPDs.find(*oit)) == end) continue;
    APDPair & p = it->second;
    if (p.ct >= 2) { // should actually always EQUAL 2!
      /* plot even here ..*/
      graph->blipPen().setColor(p.color[0]);
      graph->plotPen().setColor(p.color[0]);
      graph->plot(p.apd[0]);
                                  
      /* now plot odd here.. */
      graph->blipPen().setColor(p.color[1]);
      graph->plotPen().setColor(p.color[1]);
      graph->plot(p.apd[1]);
    }

      /* fast forward */
      graph->ffwd(static_cast<uint>
                  ((graph->secondsVisible()*graph->sampleRateHz())
                   /(channelAPDs.size() + 1)
                   - 2));
  }
  lastBeatGraphed = monitor->beatNumber();
}

APDMonitor::APDMonitor(const QColor & c1, const QColor & c2, 
                       QObject *parent, DAQSystem *_ds) 
  : QObject(parent), beat_num(0), first(0), last(0),
    color1(c1), color2(c2), colorState(c1) 
{ 
  ds = _ds;

  if (!ds) ds = *(DAQSystem::daqSystems().begin());
  

  setMasterOrder("0,1,2,3,4,5,6,7");
  
  rebuildOrder();

  connect(ds, SIGNAL(spikeThresholdSet(uint, double)),
          this, SLOT(rebuildOrder()));
  connect(ds, SIGNAL(spikeThresholdOff(uint)),
          this, SLOT(rebuildOrder()));
  connect(ds, SIGNAL(channelOpened(uint)),
          this, SLOT(rebuildOrder()));
  connect(ds, SIGNAL(channelClosed(uint)),
          this, SLOT(rebuildOrder()));

}

vector<uint> APDMonitor::orderVector() const
{
  Order inverted;
  vector<uint> ret;
  Order::const_iterator it, end;

  ret.resize(order.size());
  for (it = order.begin(), end = order.end(); it != end; it++) {
    ret[it->second] = it->first;
  }
  return ret;
}

QString APDMonitor::orderString() const
{
  vector<uint> v = orderVector();
  vector<uint>::const_iterator it, end;
  QString ret = "";

  for (it = v.begin(), end = v.end(); it != end; it++) {
    ret += QString::number(*it) + ",";
  }
  if (ret.length())
    ret.truncate(ret.length()-1);    
  return ret;
}

void APDMonitor::rebuildOrder()
{
  
  set<unsigned int> cs = ds->channelsWithSpikeThresholds();
  set<unsigned int>::iterator 
    cit = cs.begin(),
    cend = cs.end();

  order.clear();
  first = 0; last = 0;

  Order inverted;
  Order::const_iterator it, end;
  
  for (it = master_order.begin(), end = master_order.end(); it != end; it++) {
    if ( (cit = cs.find(it->first)) != cend)
      inverted[it->second] = it->first;
  }
    
  uint i;
  bool nofirst = true;
  for (i = 0, it=inverted.begin(), end=inverted.end(); it != end; it++, i++) {
    if (nofirst) { nofirst = false; first = it->second; }
    order[it->second] = i;  
    last = it->second;
  }
    
  emit orderChanged(orderString());
}

void APDMonitor::gotAPD(MCSnapShot *m)
{

  if(orderFirst() == m->apd_channel) {
    if (colorState == color1) colorState = color2;
    else colorState = color1;
    emit beatNumberChanged(++beat_num);    
  }
}

uint APDMonitor::orderOf(uint chan, bool *found) const
{
  Order::const_iterator it;

  if ( (it = order.find(chan)) == order.end()) {
    if (found) *found = false;
    return 0;
  }

  if (found) *found = true;
  return it->second;
}

void APDMonitor::setMasterOrder(const QString & order)
{
  if (!order.contains(ELECTRODE_ORDER_RE)) return;

  QStringList strings = QStringList::split(',', order);

  QStringList::iterator it = strings.begin(), end = strings.end();
  uint i;

  master_order.clear();
  set<uint> chans;
  for (i = 0; i < NumAPDGraphs; i++) // chanset for chans not in master_order
    chans.insert(i);
  

  uint chan;
  bool ok;
  for (i = 0; it != end; it++, i++) {
    chan = (*it).toUInt(&ok);
    if (chan < NumAPDGraphs 
        && ok 
        && master_order.find(chan) == master_order.end()) {
      master_order[chan] = i;
      chans.erase(chan); // remove from  chanset
    }
  }
  uint l = i;
  for(i = 0; i < NumAPDGraphs; i++) {
    if (chans.find(i) != chans.end()) { // now give remaining channels default
      master_order[i] = l++;
      chans.erase(i);
    }
  }

  emit masterOrderChanged(masterOrder());
  rebuildOrder();
}

QString APDMonitor::masterOrder() const
{
  QString ret = "";

  vector<uint> v;
  v.resize(master_order.size());
  Order::const_iterator 
    it, 
    begin = master_order.begin(), 
    end = end = master_order.end();

  for (it = begin; it != end; it++) {
    v[it->second] = it->first;
  }

  for (uint i = 0; i < v.size(); i++) {
    ret += QString::number(v[i]) + ",";
  }
  if (ret.length())
    ret.truncate(ret.length()-1);
  return ret;
}
