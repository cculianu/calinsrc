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

#include <qframe.h>
#include <qlayout.h>
#include <qlabel.h>
#include "daq_system.h"
#include "plugin.h"



class AVNStim: public QFrame, public Plugin
{
public:
  AVNStim(DAQSystem *daqSystem, QWidget *parent = 0, WFlags f = 0);
  ~AVNStim();
  const char *name() const;
  const char *description() const;
private:
  DAQSystem *daqSystem;
  int window_id;
};

extern "C" {
  Plugin *entry(DAQSystem *d) 
  { /* Top-level widget.. parent is root */
    return new AVNStim(d, 0, Qt::WType_TopLevel); 
  } 
  const char * name = "AV Node Control Experiment",
             * description =
              "Some gui controls for interfacing with the AV Node stimulation "
              "real-time module 'avn_control.o' addon developed for a custom "
              "scientific experiment at Cornell University.  Authors are "
              "David J. Christini, Ph.D and Calin A. Culianu.";
};


AVNStim::AVNStim(DAQSystem *daqSystem, QWidget *parent, WFlags f) 
  : QFrame(parent,"AVN Stim Interface", f), daqSystem(daqSystem)
{
  /* stub.. */
  QGridLayout *layout = new QGridLayout(this, 1, 1);
  QLabel *dummy_label = new QLabel(::name, this);
  layout->addWidget(dummy_label, 0, 0);
  window_id = daqSystem->windowMenuAddWindow(this);  
  show();
}

AVNStim::~AVNStim() 
{
  daqSystem->windowMenuRemoveWindow(window_id);
}

const char *
AVNStim::name() const
{
  return ::name;
}

const char *
AVNStim::description() const
{
  return ::description;
}
