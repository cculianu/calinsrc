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
struct MCLiebnitz;
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

  void toggleControl(bool); /* disables control by making stim_on be false 
                               and also setting g_adjustment_mode to 
                               MC_G_ADJ_MANUAL */

  void changeRRTarget(int);
  void changeNumRRAvg(int);
  void gAdjManualOnly(int);
  void changeG(const QString &);
  void changeDG(int);  
  void changeNomNumStims(int);

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
  SearchableComboBox *ao_channels, *ai_channels;
  QSpinBox *rr_target;
  QLineEdit *gval;

  ECGGraph *rr_graph, *stim_graph, *g_graph;

  QLabel *current_rri,     *current_rrt,    *current_stim,   *current_nom_stim,
         *current_g,   *last_beat_index, *current_rr_avg, *current_num_rr_avg, 
         *current_g2small, *current_g2big,  *current_delta_g,
         *delta_g_bar_value, *num_rr_avg_bar_value;

  QScrollBar *delta_g_bar, *num_rr_avg_bar;

  QCheckBox *g_adj_manual_only, *control_toggle; 

  TempSpooler<MCLiebnitz> *spooler;

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
