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
#include "plugin.h"

class DAQSystem;
class ECGGraph;
class QWidget;
struct AVNShared;
class QGridLayout;
class QLabel;
class SearchableComboBox;
class QTimer;
class QSpinBox;
class QLineEdit;

class AVNStim: public QObject, public Plugin
{
  Q_OBJECT

public:
  AVNStim(DAQSystem *daqSystem);
  ~AVNStim();  
  const char *name() const;
  const char *description() const;

private slots:
  void periodic(); /* does stuff periodically.. called by the timer */
  void setFakeStim(int state); 
  void changeAOChan(const QString &); /* reads the current state of 
                                         ao_channels and tells the realtime 
                                         process about our new desired ao chan
                                      */
  void changeAIChan(const QString &); /* applies combo box change to 
                                         rt process avn_stim.o */
  void synchAIChan(); /* synchs combo box with list of open ai chans from
                         rt_process.o's shared memory struct SharedStuff */
  void changeRRTarget(int);
  void gAdjManualOnly(int);
  void changeG(const QString &);

  void save();
  void saveAs();
  void safelyQuit();

private:

  /* DAQSystem-related stuff */
  DAQSystem *daqSystem(); /* returns parent() dynamic_cast to DAQSystem* */
  int window_id;
  bool need_to_save;
  QString outFile;

  void *__data; /* opaque type encapsulting the data to be saved to disk */

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

  QLabel *current_rri, *current_rrt, *current_stim, *current_g, *last_beat_index;

  void addAxisLabels(); /* puts the axis labels for the above graphs in */
  void populateAOComboBox();  
  void synchAOChan();  /* synch the ao_channels combobox with the kernel */
  

  /* Module-related stuff */
  AVNShared *shm;
  int in_fifo, out_fifo; /* fd's of each of the fifos */
  QTimer *timer;

  void moduleAttachDetach(); /* attaches to avn_stim.o's fifo and shm */
  bool needFifo() const { return (in_fifo < 0 || out_fifo < 0); }
  void readInFifo(); /* reads in data off the fifo from the avn_stim module */
  
};