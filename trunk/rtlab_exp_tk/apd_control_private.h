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

public:
  APDcontrol(DAQSystem *daqSystem);
  virtual ~APDcontrol();  
  virtual const char *name() const;
  virtual const char *description() const;

private slots:
  void periodic(); /* does stuff periodically.. called by the timer */
  void changeAOChan(const QString &); /* reads the current state of 
                                         ao_channels and tells the realtime 
                                         process about our new desired ao chan
                                      */
  void changeAIChan(const QString &); /* applies combo box change to 
                                         rt process avn_stim.o */
  void synchAIChan(); /* synchs combo box with list of open ai chans from
                         rt_process.o's shared memory struct SharedStuff */

  void toggleControl(bool); /* disables control by making control_on be false 
                               and also setting g_adjustment_mode to 
                               MC_G_ADJ_MANUAL */
  void toggleOnlyNegativePerturbations(bool); /* If ON, then allow only negative perturbations
                                                                                if OFF both positive and negative control perturbations */

  void togglePacing(bool);    /* disables pacing by making pacing_on false */
  void changeNominalPI(int); /* change the nominal Pacing Interval */
  void toggleUnderlying(bool); /* toggles continue_underlying, which, if control is active,
			     continues to pace at the pacing interval, i.e., the pacing 
                                                     continues oblivious to control. if continue_underlying 
                                                     is false, then after a given control stimulus, the next stimulus
                                                    will occur at an interval equal to the pacing interval */
  void toggleTargetShorter(bool); /* toggles target_shorter, which, if control is active,
			           sets the initial control pacing interval equal to a value
                                                           that will obtain an APD equal to the short alternating APD.
				   i.e., two short APDs in a row. */  
  void gAdjManualOnly(int);
  void changeG(const QString &);
  void changeDG(int);  
  void changeAPDxx(const QString &);

  void save();
  void saveAs();
  void safelyQuit();

  void graphHasChangedRange(int range); /* uses QObject::sender to determine
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

  ECGGraph *apd_graph, *delta_pi_graph, *g_graph;
  /* Dummy Widget that contains 3 qlabels, 1 per axis label */
  QWidget *apd_graph_labels, *delta_pi_graph_labels, *g_graph_labels;
  /* Combo box atop each graph that controls graph ranges.. this gets populated
     with GraphRangeSettings for each combo box, see apd_control.cpp */
  QComboBox *apd_range_ctl, *delta_pi_range_ctl, *g_range_ctl;
  /* this stuff defines the contents of the above combo boxes and also
     controls the range settings of the ECGGraph * graphs above */
  struct GraphRangeSettings { double min; double max; };
  static const int n_apd_ranges, n_delta_pi_ranges, n_g_ranges;
  static const GraphRangeSettings apd_ranges[], delta_pi_ranges[], g_ranges[];
  /* populates the QComboBoxes above with the stuff from the 
     static const GraphRangeSettings structs (see apd_control.cpp for actual
     min/max values for each range setting ) */
  void buildRangeComboBoxesAndConnectSignals(); 
                                                   

  QLabel *gui_indicator_pi, *gui_indicator_delta_pi, *gui_indicator_apd, 
    *gui_indicator_di, *gui_indicator_v_apa, *gui_indicator_v_baseline, 
    *gui_indicator_ap_ti, *gui_indicator_ap_tf,
    *gui_indicator_g_val, *gui_indicator_delta_g, *gui_indicator_consec_alternating, 
    *gui_indicator_delta_g_bar_value;

  SearchableComboBox *ao_channels, *ai_channels;
  QSpinBox *nominal_pi;
  QLineEdit *gval, *apd_xx_edit_box;
  QScrollBar *delta_g_bar;
  QCheckBox *pacing_toggle, *control_toggle, *only_negative_toggle, *underlying_toggle, 
	        *target_shorter_toggle, *g_adj_manual_only;

  TempSpooler<MCSnapShot> *spooler;

  /* puts the axis labels for the above graphs in
     if rebuildOnlyNull is true, then check if the graph already has labels,
     before blindly re-creating new ones */
  void addAxisLabels(bool rebuildOnlyNull = false); 
  void populateAOComboBox();  
  void synchAOChan();  /* synch the ao_channels combobox with the kernel */

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
};
