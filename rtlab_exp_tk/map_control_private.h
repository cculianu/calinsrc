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
class QTimer;
class QSpinBox;
class QLineEdit;
class QScrollBar;
class QCheckBox;
template<class T> class TempSpooler;

class MapControl: public QObject, public Plugin
{
  Q_OBJECT

public:
  MapControl(DAQSystem *daqSystem);
  virtual ~MapControl();  
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

  void save();
  void saveAs();
  void safelyQuit();

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

  QLabel *gui_indicator_pi, *gui_indicator_delta_pi, *gui_indicator_apd, 
    *gui_indicator_previous_apd, *gui_indicator_apd_fp,
    *gui_indicator_di, *gui_indicator_previous_di, *gui_indicator_vmax, *gui_indicator_vmin, 
    *gui_indicator_ap_ti, *gui_indicator_ap_t90,
    *gui_indicator_g, *gui_indicator_g_val, *gui_indicator_delta_g, *gui_indicator_consec_alternating, 
    *gui_indicator_last_beat_index, *gui_indicator_delta_g_bar_value;

  SearchableComboBox *ao_channels, *ai_channels;
  QSpinBox *nominal_pi;
  QLineEdit *gval;
  QScrollBar *delta_g_bar;
  QCheckBox *g_adj_manual_only, *control_toggle, *pacing_toggle, *underlying_toggle, 
	        *target_shorter_toggle;

  TempSpooler<MCSnapShot> *spooler;

  void addAxisLabels(); /* puts the axis labels for the above graphs in */
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
