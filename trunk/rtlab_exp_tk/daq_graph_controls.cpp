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

#include <limits.h>

#include <qmainwindow.h>
#include <qlayout.h>
#include <qhbox.h>
#include <qlabel.h>
#include <qspinbox.h>
#include <qcombobox.h>
#include <qcheckbox.h>

#include <qworkspace.h>
#include <qpixmap.h>
#include <qtooltip.h>

#include "daq_graph_controls.h"
#include "daq_channel_params.h"
#include "ecggraphcontainer.h"
#include "daq_images.h"
#include "exception.h"


const int 
  DAQGraphControls::secsVisibleStep   = 5, // used for GUI spinbox
  DAQGraphControls::minSecsVisible    = 0, // used for GUI spinbox  
  DAQGraphControls::maxSecsVisible    = 100, // used for GUI spinbox
  DAQGraphControls::minSpikeBlanking  = 10, // used for GUI spinbox
  DAQGraphControls::maxSpikeBlanking  = 1000000, // used for GUI spinbox
  DAQGraphControls::spikeBlankingStep = 10; // used for GUI spinbox

const uint DAQGraphControls::invalid_current = (UINT_MAX);

DAQGraphControls::
DAQGraphControls ( const QString & label, QMainWindow * mainWindow, 
                   QWorkspace *ws, QWidget * parent, bool newLine = FALSE, 
                   const char * name = 0, WFlags f = 0 ) 
  : QToolBar(label, mainWindow, parent, newLine, name, f),
    current(invalid_current), ws(ws)
{ 
  init(); 
}

DAQGraphControls::~DAQGraphControls() {}


uint 
DAQGraphControls::
addGraph( const QString & label, ECGGraphContainer * g, 
          const ComediSubDevice & dev, int id)
{
  uint ret;
  if (id < 0) {
    /* we auto-generate an id.. best thing is to start with graph channelId */
    ret = g->channelId;
    while (graphs.find(ret) != graphs.end()) ret++;
  } else {
    ret = id;
  }

  DAQChannelParams parm;

  parm.label = label;
  parm.id = ret;
  parm.buildRangeStrings(&dev);
  parm.setNull(false);

  addGraph(g, parm);

  return ret;
}

uint 
DAQGraphControls::
addGraph( ECGGraphContainer * g, const DAQChannelParams & p)
{
  uint ret = p.id;

  GraphParam parm(p);

  parm.graph = g;
  parm.setNull(false);

  graphs[ret] = parm;

  activateGraph(g);


  if (p.spikeOn) {
    g->graph->setSpikeThreshold(p.spikeThold); 
    /* signal from graph will propagate this information out to the 
       Shared Memory for the rt_process? */
  }

  /* make sure the graph itself gets these values */
  setRange(p.rangeSetting);
  setSpikePolarity(p.spikePolarity);
  setSpikeBlanking(p.spikeBlanking);
  setSecondsVisible(p.secondsVisible);
  setAxesOn(p.axesOn);
  setStatusBarOn(p.statusBarOn);

  return ret;
}

void DAQGraphControls::removeGraph ( const QString & label ) 
{
  GraphParams::iterator it;

  for (it = graphs.begin(); it != graphs.end(); it++) 
    if (it->second.label == label) removeGraph(it->first);
  
}

void DAQGraphControls::removeGraph ( const ECGGraphContainer * g ) 
{
  GraphParams::iterator it;

  for (it = graphs.begin(); it != graphs.end(); it++) 
    if (it->second.graph == g) removeGraph(it->first);
  
}

void DAQGraphControls::removeGraph ( uint id ) 
{
  if (graphs.find(id) == graphs.end()) return;

  graphs.erase(id);
  if (graphs.size() == 0) 
    /* if we have no more graphs, obviously turn off the GUIs */
    disableGUIs();
  
}

const ECGGraphContainer * DAQGraphControls::currentGraph() const
{ 
  return ( graphs.find(current) != graphs.end() 
           ? graphs.find(current)->second.graph 
           : 0 ); 
}

ECGGraphContainer * DAQGraphControls::currentGraph() 
{ 
  return ( graphs.find(current) != graphs.end() 
           ? graphs.find(current)->second.graph 
           : 0 ); 
}

uint DAQGraphControls::currentGraphId() const { return current; }


void DAQGraphControls::activateGraph( uint id )
{
  GraphParams::iterator it = graphs.find(id);

  if (it != graphs.end()) {
    it->second.graph->show();
    it->second.graph->raise();
    it->second.graph->setFocus();
  }
  windowActivated(graphs[id].graph);
}

void DAQGraphControls::activateGraph( ECGGraphContainer * g)
{
  GraphParams::iterator it;

  for (it = graphs.begin(); it != graphs.end(); it++) {
    if (it->second.graph == g) activateGraph(it->first);
  }
}

const DAQChannelParams & DAQGraphControls::getParams(uint id) const
{
  GraphParams::iterator it = graphs.find(id);
  
  if (it != graphs.end()) return it->second;
  return DAQChannelParams::null;  
}

const DAQChannelParams & 
DAQGraphControls::getParams(const ECGGraphContainer *g) const
{
  GraphParams::iterator it;
  
  for (it = graphs.begin(); it != graphs.end(); it++) 
    if (it->second.graph == g) return getParams(it->first);
  return DAQChannelParams::null;
}


void DAQGraphControls::setRange( int index )
{
  uint r = static_cast<uint>(index);

  /* invalid range check */
  if ( r > graphs[current].rangeSettings.size() ) return;

  graphs[current].rangeSetting = r;
  double min, max;
  ComediRange::Unit ru;
  ComediRange::parseString(graphs[current].rangeSettings[r], min, max, ru);  

  if (ru == ComediRange::MilliVolts) { min *= 1000.0; max *= 1000.0; }
  
  graphs[current].graph -> graph -> setRange(min, max);

  if (rangeComboBox->currentItem() != index) 
    rangeComboBox->setCurrentItem(index);
  
  emit rangeChanged(current, r);
  emit rangeChanged(current, graphs[current].rangeSettings[r]);
  emit rangeChanged(current, min, max);
}

void 
DAQGraphControls::setSecondsVisible( int v )
{
  uint value = static_cast<uint>(v);

  graphs[current].graph->graph->setSecondsVisible(value);
  graphs[current].secondsVisible = value;

  if (secondsVisibleBox->value() != v) secondsVisibleBox->setValue(v);

  emit secondsVisibleChanged(current, value);  
}

void 
DAQGraphControls::setSpikeBlanking ( int m )
{
  uint ms = static_cast<uint>(m);

  graphs[current].spikeBlanking = ms;

  if (spikeBlanking->value() != m) spikeBlanking->setValue(m);
  
  emit spikeBlankingChanged(current, ms);
}

void
DAQGraphControls::toggleSpikePolarity()
{
  setSpikePolarity(graphs[current].spikePolarity == Positive 
                   ? Negative 
                   : Positive);
}

void DAQGraphControls::setSpikePolarity(SpikePolarity p)
{
  graphs[current].spikePolarity = p;
  setSpikePolarityButton(p);
  emit spikePolarityChanged(current, p);
}

void DAQGraphControls::setSpikePolarityButton(SpikePolarity p)
{
  switch (p) {
  case Positive:
    polarityButton->setPixmap(QPixmap( DAQImages::spike_plus_img ) );
    break;
  case Negative:
    polarityButton->setPixmap(QPixmap( DAQImages::spike_minus_img ) );
    break;
  }
}

void DAQGraphControls::togglePause()
{
  graphs[current].graph->pauseUnpause();
  graphs[current].paused = graphs[current].graph->isPaused();
  setPausePlayButton(graphs[current].paused);  
  emit pausedUnpaused(current, graphs[current].paused);
}

void DAQGraphControls::setPausePlayButton(bool paused)
{
  if ( paused != pauseButton->isOn() )
    pauseButton->toggle();    
}

void DAQGraphControls::setAxesOn(bool on)
{
  axesOn->setChecked(on);

  graphs[current].axesOn = on;
  
  graphs[current].graph->setXAxis(on);
  //graphs[current].graph->setYAxis(on);

  emit axesOnChanged(on);
}

void DAQGraphControls::setStatusBarOn(bool on)
{
  statusBarOn->setChecked(on);

  graphs[current].statusBarOn = on;
  graphs[current].graph->setStatusBar(on);

  emit statusBarOnChanged(on);  
}


void
DAQGraphControls::
init ()
{
  connect(ws, SIGNAL(windowActivated(QWidget *)), 
          this, SLOT(windowActivated(QWidget *)));
  
  this->setFrameStyle(StyledPanel | Raised);

  /* actually build GUI elements here */
  constructGUIs();

  sti(); // setup the signals

}


void DAQGraphControls::sti()
{
  connect(pauseButton, SIGNAL(toggled(bool)), this, SLOT(togglePause()));
  connect(rangeComboBox, SIGNAL(activated(int)), this, SLOT(setRange(int)));
  connect(secondsVisibleBox, SIGNAL(valueChanged(int)), 
          this, SLOT(setSecondsVisible(int)));
  connect(polarityButton, SIGNAL(clicked()), 
          this, SLOT(toggleSpikePolarity()));
  connect(spikeBlanking, SIGNAL(valueChanged(int)),
          this, SLOT(setSpikeBlanking(int)));
  connect(axesOn, SIGNAL(toggled(bool)),
          this, SLOT(setAxesOn(bool)));
  connect(statusBarOn, SIGNAL(toggled(bool)),
          this, SLOT(setStatusBarOn(bool)));
}

void DAQGraphControls::cli()
{
  disconnect(pauseButton, SIGNAL(toggled(bool)), this, SLOT(togglePause()));
  disconnect(rangeComboBox, SIGNAL(activated(int)), this, SLOT(setRange(int)));
  disconnect(secondsVisibleBox, SIGNAL(valueChanged(int)), 
             this, SLOT(setSecondsVisible(int)));
  disconnect(polarityButton, SIGNAL(clicked()), 
             this, SLOT(toggleSpikePolarity()));
  disconnect(spikeBlanking, SIGNAL(valueChanged(int)),
             this, SLOT(setSpikeBlanking(int)));
  disconnect(axesOn, SIGNAL(toggled(bool)),
             this, SLOT(setAxesOn(bool)));
  disconnect(statusBarOn, SIGNAL(toggled(bool)),
             this, SLOT(setStatusBarOn(bool)));

}

void DAQGraphControls::constructGUIs()
{
  // graph's name in the top left
  graphNameLabel = new QLabel("", this);
  graphNameLabel->setScaledContents(false);

  guisToDisableEnable.push_back(graphNameLabel);

  pauseButton = new QPushButton ( this );
  pauseButton->setPixmap( QPixmap(DAQImages::pause_img) );
  pauseButton->setMaximumSize(pauseButton->sizeHint());
  pauseButton->setToggleButton(true);
  QToolTip::add(pauseButton, "Pauses the graph of this channel, but not the "
                             "actual data acquisition");

  guisToDisableEnable.push_back(pauseButton);
  
  addSeparator();

  QLabel *tmpLabel = new QLabel ("Change Scale: ", this);  

  guisToDisableEnable.push_back(tmpLabel);

  rangeComboBox = new QComboBox(this, "Scale Box");
  const char *rangeComboBoxTip = "Use this to change graph scale (Y Axis Range).";
  QToolTip::add(rangeComboBox, rangeComboBoxTip);
  QToolTip::add(tmpLabel, rangeComboBoxTip);

  guisToDisableEnable.push_back(rangeComboBox);

  addSeparator();

  tmpLabel =  new QLabel("Secs. Visible", this); 

  guisToDisableEnable.push_back(tmpLabel);

  /* we can do the above because of qt's 'quasi-garbage collection'  */
  secondsVisibleBox = new QSpinBox(0, maxSecsVisible, secsVisibleStep, this);

  const char * secondsVisibleBoxTip =  
    "Use this to change the number of seconds visible in your graph "
    "(X Axis Scale).";
  QToolTip::add(tmpLabel, secondsVisibleBoxTip);
  QToolTip::add(secondsVisibleBox, secondsVisibleBoxTip);
  
  guisToDisableEnable.push_back(secondsVisibleBox);

  addSeparator();

  tmpLabel = new QLabel("Spike 'Polarity': ", this);

  QString tmpStr("Set the spike polarity by selecting either positive or "
                 "negative spike polarity.");

  QToolTip::add(tmpLabel, tmpStr);
  
  guisToDisableEnable.push_back(tmpLabel);

  
  polarityButton = new QPushButton( this );
  polarityButton->setPixmap(QPixmap(DAQImages::spike_plus_img));
  QToolTip::add(polarityButton, tmpStr);

  guisToDisableEnable.push_back(polarityButton);

  addSeparator();

  tmpLabel = new QLabel("  Spike Blanking (ms):", this);
  guisToDisableEnable.push_back(tmpLabel);

  spikeBlanking = new QSpinBox(minSpikeBlanking, maxSpikeBlanking, 
                               spikeBlankingStep, this);


  tmpStr=QString("Set the spike blanking, which is defined as the number of\n"
                 "milliseconds to wait before detecting a new spike.  If\n"
                 "this is set too low and you are constantly getting spikes,\n"
                 "you may not be able to keep track of them as they will\n"
                 "come in too fast.  If this is set too high you may lose\n"
                 "some spikes you would have otherwise been interested in.");
  QToolTip::add(tmpLabel, tmpStr);
  QToolTip::add(spikeBlanking, tmpStr);

  guisToDisableEnable.push_back(spikeBlanking);

  addSeparator();

  axesOn = new QCheckBox("X-Axis  ", this);
  statusBarOn = new QCheckBox("Status bar  ", this);

  QToolTip::add(axesOn, "Toggle the x-axis (seconds indicator) of the graph.");
  QToolTip::add(statusBarOn, "Toggle the status bar at the bottom of the graph.");

  guisToDisableEnable.push_back(axesOn);
  guisToDisableEnable.push_back(statusBarOn);
  

  disableGUIs();
}

void DAQGraphControls::disableGUIs()
{
  WidgetVector::iterator it = guisToDisableEnable.begin();

  while (it != guisToDisableEnable.end()) {
    (*it)->setEnabled(false);
    it++;
  }
}

void DAQGraphControls::enableGUIs()
{
  WidgetVector::iterator it = guisToDisableEnable.begin();

  while (it != guisToDisableEnable.end()) {
    (*it)->setEnabled(true);
    it++;
  }
}

void DAQGraphControls::populateGUIs()
{
  cli(); /* turns off all gui signals that come to this instance's private 
            slots */

  /* todo : do the GUI swap here based on this window's state... */
  graphNameLabel->setText(graphs[current].label);

  setPausePlayButton(graphs[current].paused);
  populateRangeComboBox();  
  rangeComboBox->setCurrentItem(graphs[current].rangeSetting);
  secondsVisibleBox->setValue(graphs[current].secondsVisible);
  polarityButton->setPixmap ( QPixmap(graphs[current].spikePolarity == Positive
                                      ? DAQImages::spike_plus_img
                                      : DAQImages::spike_minus_img) );
  spikeBlanking->setValue(graphs[current].spikeBlanking);
  axesOn->setChecked(graphs[current].axesOn);
  statusBarOn->setChecked(graphs[current].statusBarOn);
  sti();  /* turns on private slot-bound signal processing */
}

void DAQGraphControls::
populateRangeComboBox()
{
  
  rangeComboBox->clear();

  GraphParam::RangeSettings & rs = graphs[current].rangeSettings;

  uint i;
  
  for (i = 0; i < rs.size(); i++) 
    rangeComboBox->insertItem(rs[i], i);

}

void
DAQGraphControls::
windowActivated(QWidget *w)
{
  ECGGraphContainer *g;

  if ( !graphs.size() ) {
    disableGUIs();
    return;
  } else if (!(g = dynamic_cast<ECGGraphContainer *>(w))) {
    return;
  }

  GraphParams::iterator it;
  
  for (it = graphs.begin(); it != graphs.end(); it++) 
    if (it->second.graph == g) break;  
  if ( it == graphs.end() ) return;

  current = it->first;
  populateGUIs();
  enableGUIs();
}

