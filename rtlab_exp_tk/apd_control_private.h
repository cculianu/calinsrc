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

#include <qobject.h>
#include <qstring.h>
#include <qsignalmapper.h>
#include "apd_control.h"
#include "plugin.h"

class DAQSystem;
class ShmController;
class ECGGraph;
class QWidget;
struct MCShared;
struct MCSnapShot;
class QGridLayout;
class QLabel;
class SearchableComboBox;
class QComboBox;
class QTimer;
class QSpinBox;
class QLineEdit;
class QScrollBar;
class QCheckBox;
template<class T> class TempSpooler;

class APDcontrol: public QObject, public Plugin
{
  Q_OBJECT

#define NumAPDGraphs 8

public:
  APDcontrol(DAQSystem *daqSystem);
  virtual ~APDcontrol();  
  virtual const char *name() const;
  virtual const char *description() const;

 private:
  /*SignalMappers let the slots for each graph widget know which
    graph the signal originated from*/
  QSignalMapper *ai_channels_mapper;
  QSignalMapper *nominal_pi_mapper;
  QSignalMapper *gval_mapper;
  /*QSignalMapper *apd_xx_edit_box_mapper;*/
  QSignalMapper *delta_g_bar_mapper;
  QSignalMapper *pacing_toggle_mapper;
  QSignalMapper *control_toggle_mapper;
  QSignalMapper *underlying_toggle_mapper;
  QSignalMapper *only_negative_toggle_mapper;
  QSignalMapper *target_shorter_toggle_mapper;
  QSignalMapper *g_adj_manual_only_mapper;

private slots:
  void periodic(); /* does stuff periodically.. called by the timer */
  void changeAIChan(int); /* applies combo box change to 
                                         rt process avn_stim.o */
  void synchAIChan(); /* synchs combo box with list of open ai chans from
                         rt_process.o's shared memory struct SharedStuff */

  void toggleControl(int); /* disables control by making control_on be false 
                               and also setting g_adjustment_mode to 
                               MC_G_ADJ_MANUAL */
  void toggleOnlyNegativePerturbations(int); /* If ON, then allow only negative perturbations
                                                                                if OFF both positive and negative control perturbations */

  void toggleLinkToAO0(bool);
  void changeAO0_AO1_CondTime(int);

  void togglePacing(int);    /* disables pacing by making pacing_on false */
  void changeNominalPI(int); /* change the nominal Pacing Interval */
  void toggleUnderlying(int); /* toggles continue_underlying, which, if control is active,
			     continues to pace at the pacing interval, i.e., the pacing 
                                                     continues oblivious to control. if continue_underlying 
                                                     is false, then after a given control stimulus, the next stimulus
                                                    will occur at an interval equal to the pacing interval */
  void toggleTargetShorter(int); /* toggles target_shorter, which, if control is active,
			           sets the initial control pacing interval equal to a value
                                                           that will obtain an APD equal to the short alternating APD.
				   i.e., two short APDs in a row. */  
  void gAdjManualOnly(int);
  void changeG(int);
  void changeDG(int);  
  void changeAPDxx(const QString &);

  void save();
  void saveAs();
  void safelyQuit();

  void graphHasChangedRange(int); /* uses QObject::sender to determine
                                           correct graph to apply range change
                                           to.  See apd_range_ctl, et al 
                                           below */

private:

  /* DAQSystem-related stuff */
  DAQSystem *daqSystem(); /* returns parent() dynamic_cast to DAQSystem* */
  int window_id;
  bool need_to_save;
  QString outFile;

  const ShmController *daq_shmCtl; /* from daqSystem */

  /* Layout/UI related-stuff */
  QWidget *window;  /* this contains the whole shebang */
  QWidget *graphs, *controls; /* dummy containers for all the graphs */
  QGridLayout *masterlayout,   /* 1,2 */
              *graphlayout,    /* 6,2 */
              *controlslayout;

  ECGGraph *apd_graph[NumAPDGraphs+1];    /* 1 extra for graph showing all electrodes*/
  /* Dummy Widget that contains 3 qlabels, 1 per axis label */
  QWidget *apd_graph_labels[NumAPDGraphs+1];
  /* Combo box atop each graph that controls graph ranges.. this gets populated
     with GraphRangeSettings for each combo box, see apd_control.cpp */
  QComboBox *apd_range_ctl[NumAPDGraphs+1];
  /* this stuff defines the contents of the above combo boxes and also
     controls the range settings of the ECGGraph * graphs above */
  struct GraphRangeSettings { double min; double max; };
  static const int n_apd_ranges;
  static const GraphRangeSettings apd_ranges[];
                                                   
  QLabel *gui_indicator_pi[NumAOchannels], 
    *gui_indicator_delta_pi[NumAOchannels], 
    *gui_indicator_apd[NumAOchannels], 
    *gui_indicator_di[NumAOchannels], 
    *gui_indicator_v_apa[NumAOchannels], 
    *gui_indicator_v_baseline[NumAOchannels], 
    *gui_indicator_ap_ti[NumAOchannels], 
    *gui_indicator_ap_tf[NumAOchannels],
    *gui_indicator_g_val[NumAOchannels], 
    *gui_indicator_delta_g[NumAOchannels], 
    *gui_indicator_consec_alternating[NumAOchannels], 
    *gui_indicator_delta_g_bar_value[NumAOchannels];

  SearchableComboBox *ai_channels[NumAOchannels];
  QSpinBox *nominal_pi[NumAOchannels], *ao0_ao1_cond_time;
  QLineEdit *gval[NumAOchannels], *apd_xx_edit_box;
  QScrollBar *delta_g_bar[NumAOchannels];
  QCheckBox *pacing_toggle[NumAOchannels], 
    *link_to_ao0_toggle,
    *control_toggle[NumAOchannels], 
    *only_negative_toggle[NumAOchannels], 
    *underlying_toggle[NumAOchannels], 
    *target_shorter_toggle[NumAOchannels], 
    *g_adj_manual_only[NumAOchannels];

  TempSpooler<MCSnapShot> *spooler;

  /* puts the axis labels for the above graphs in
     if rebuildOnlyNull is true, then check if the graph already has labels,
     before blindly re-creating new ones */
  void addAxisLabels(int,int,int); 
  static void determineGraphRowColumn(int, int &, int &);
  /* populates the QComboBoxes above with the stuff from the 
     static const GraphRangeSettings structs (see apd_control.cpp for actual
     min/max values for each range setting ) */
  void buildRangeComboBoxesAndConnectSignals(int); 

  /* Module-related stuff */
  MCShared *shm;
  enum RTOSUsed { RTAI, RTLinux, Unknown };
  RTOSUsed rtosUsed;
  int fifo; /* fd's of the input fifo */
  QTimer *timer;

  void determineRTOS();
  void moduleAttach(); /* attaches to avn_stim.o's fifo and shm */
  void moduleDetach(); /* closes the fifo fd and detached from shm */
  bool needFifo() const { return (fifo < 0); }
  void readInFifo(); /* reads in data off the fifo from the avn_stim module */

  /* static data that is generated at compile-time and goes into
     the output textfile as a header */
  static const char * fileheader;

  /* Simply prints delta G to a string uses the constant
     MC_DELTA_G_GUI_PRECISION defined in apd_control.h to control the sprintf*/
  static QString stringifyDeltaG(double g); 
};
