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
#include <qvbox.h>
#include <qhbox.h>
#include <qlabel.h>
#include <qevent.h>
#include <qregexp.h>
#include <qtooltip.h>

#include <stdio.h>
#include <math.h>

#include "ecggraphcontainer.h"


/* ugly non-class-specific static constants but this saves needing to 
   duplicate this work in the .h file */
static const QString 
  MOUSE_POS_FORMAT("Mouse pos: %1 V at scan %2"),
  CURRENT_INDEX_FORMAT("Scan Index: %1"),
  SPIKE_THOLD_FORMAT("Spike Threshold: %1 V"),
  LAST_SPIKE_FORMAT("Last Spike %1 V at %2"),
  SPIKE_FREQUENCY_FORMAT ("Spike Freq: %1 BPM (%2 hz or %3 ms/spike)");
  

/* used for string parsing and building in the combo box
   see also enum RangeUnit */
const QString ECGGraphContainer::unitStrings[3] =  { "V", "mV" };

  /** Pass in a graph to become the container of.  Graph gets reparented
      to be a child of this class! */
ECGGraphContainer::ECGGraphContainer(ECGGraph *graph, 	
                                     uint channelId,
                                     QWidget *parent, 
                                     const char *name, 
                                     WFlags flags,
                                     scan_index_t scanIndexStatusIncrement, 
                                     uint seconds_visible_step) 
  : 
    QFrame (parent, name, flags | WDestructiveClose), 
    graph(graph),
    channelId(channelId),
    seconds_visible_step(seconds_visible_step),
    scan_index_threshold(scanIndexStatusIncrement), 
    last_scan_index((scan_index_t)-scan_index_threshold),
    last_spike_index(0)
    
{  
  if (seconds_visible_step == 0) seconds_visible_step = 1;

  static const QString yAxisLabelFormat( "%1 V" );
  QHBox *tmpBox;
  
  this->setFrameStyle(StyledPanel);
  
  // this is the master layout for this widget
  layout = new QGridLayout (this, 2, 2, 1, 1);
  

  // the controls on top
  controlsBox = new QHBox(this);
  controlsBox->setFrameStyle(StyledPanel | Raised);
  controlsBox->setMargin(1);
  controlsBox->setSpacing(6);

  // graph's name in the top left
  graphNameLabel = new QLabel((name ? name : ""), controlsBox);
  graphNameLabel->setAlignment(AlignRight | AlignTop);  
  graphNameLabel->setMargin(1);
  graphNameLabel->setFrameStyle(QFrame::StyledPanel | QFrame::Raised);

  // the range settings control
  tmpBox = new QHBox(controlsBox);
  tmpBox->setFrameStyle(StyledPanel | Raised);
  tmpBox->setMargin(1);
  tmpBox->setSpacing(2);

  QLabel *tmpLabel = new QLabel ("Change Scale: ", tmpBox);  
  rangeComboBox = new QComboBox(tmpBox, 
                                QString("%1 Scale Box").arg(graph->name()));
  QToolTip::add(rangeComboBox, "Use this to change graph scale (Y Axis Range).");
  QToolTip::add(tmpLabel, "Use this to change graph scale (Y Axis Range).");

  controlsBox->setStretchFactor(rangeComboBox, 1);  
  layout->addMultiCellWidget (controlsBox, 0, 0, 0, 1);

  connect(rangeComboBox,  
          SIGNAL(activated(const QString &)),
          this, 
          SLOT (rangeChange(const QString &))); 

  // the number of seconds visible spin box up top
  tmpBox = new QHBox(controlsBox);
  tmpBox->setFrameStyle(StyledPanel | Raised);
  tmpBox->setMargin(1);
  tmpBox->setSpacing(2);

  tmpLabel = 
    new QLabel("Secs. Visible", tmpBox); 
  /* we can do the above because of qt's 'quasi-garbage collection'  */
  secondsVisibleBox = new QSpinBox(0, 100, seconds_visible_step, tmpBox);
  secondsVisibleBox->setValue(graph->secondsVisible());
  QToolTip::add(tmpLabel, "Use this to change the number of seconds visible "
		          "in your graph (X Axis Scale).");
  QToolTip::add(secondsVisibleBox, "Use this to change the number of seconds "
		                   "visible in your graph (X Axis Scale).");
  connect(secondsVisibleBox, SIGNAL(valueChanged ( int )),
          this, SLOT(setSecondsVisible(int)));
  

  // the spike polarity box
  tmpBox = new QHBox(controlsBox); // we rely on auto-gc
  tmpBox->setFrameStyle(StyledPanel | Raised);
  tmpBox->setMargin(1);
  tmpBox->setSpacing(2);

  spikePolarityLabel = new QLabel("Spike 'Polarity': ", tmpBox);
  spikePolarityButtons = new QButtonGroup();
  spikePolarityButtons->setRadioButtonExclusive(true);
  spikePolarityButtons->hide();
  polarityPlusButton = new QRadioButton("Positive", tmpBox);
  polarityMinusButton = new QRadioButton("Negative", tmpBox);
  spikePolarityButtons->insert(polarityPlusButton, Positive);
  spikePolarityButtons->insert(polarityMinusButton, Negative);

  QToolTip::add(spikePolarityLabel, 
                "Set the spike polarity by selecting either positive or "
                "negative spike polarity.");
  QToolTip::add(polarityPlusButton,
                "A positive spike polarity means that a spike is detected if "
                "the amplitude is greater than or equal to the spike "
                "threshold.");
  QToolTip::add(polarityMinusButton,
                "A negative spike polarity means that a spike is detected if "
                "the amplitude is less than or equal to the spike "
                "threshold.");
  connect(spikePolarityButtons, SIGNAL(clicked(int)), 
          this, SLOT(emitSpikePolarity(int)));
  
  QString tmpStr("Set the spike blanking, which is defined as the number of\n"
                 "milliseconds to wait before detecting a new spike.  If\n"
                 "this is set too low and you are constantly getting spikes,\n"
                 "you may not be able to keep track of them as they will\n"
                 "come in too fast.  If this is set too high you may lose\n"
                 "some spikes you would have otherwise been interested in.");

  tmpLabel = new QLabel("  Spike Blanking (ms):", tmpBox);
  spikeBlanking = new QSpinBox(10, 1000000, 10, tmpBox);
  QToolTip::add(tmpLabel, tmpStr);
  QToolTip::add(spikeBlanking, tmpStr);
  connect(spikeBlanking, SIGNAL(valueChanged(int)), 
          this, SLOT (emitSpikeBlanking(int))); 
  
  /* y axis labels -- these get auto-updated whenever the graph emits signal 
     rangeChanged */
  labelBox = new QVBox(this);
  labelBox->setFrameStyle(StyledPanel | Raised);
  labelBox->setMargin(2);

  topYLabel = new QLabel(labelBox);
  middleYLabel = new QLabel(labelBox);
  bottomYLabel = new QLabel(labelBox);
  topYLabel->setAlignment(AlignRight | AlignTop);
  middleYLabel->setAlignment(AlignRight | AlignVCenter);
  bottomYLabel->setAlignment(AlignRight | AlignBottom);

  /* so that labels and things are always truly reflecting what the graph is 
     doing... */
  connect(graph, 
          SIGNAL(rangeChanged(double, double)), 
          this, 
          SLOT(updateYAxisLabels(double, double)));  
  connect (graph,
           SIGNAL(eitherClicked(void)),
           this,
           SLOT(mouseDownInGraph(void)));
  connect (graph,
           SIGNAL(eitherReleased(void)),
           this,
           SLOT(mouseUpInGraph(void)));
  connect (graph,
           SIGNAL(eitherClicked(void)),
           graph,
           SLOT(unsetSpikeThreshold(void)));

  connect (graph,
           SIGNAL(secondsVisibleChanged(int)),
           secondsVisibleBox,
           SLOT(setValue(int)));

  graph->reparent(this, QPoint(0,0) );

  // re-synch the labels to the graph, just to be sure...
  // setRange implicitly emits rangeChanged()
  graph->setRange(graph->rangeMin(), graph->rangeMax());
  QToolTip::add(graph, "Click+Drag to set a spike threshhold.");
  
  layout->addWidget (labelBox, 1, 0);
  layout->addWidget (graph, 1, 1);

  layout->setColStretch(1,1); // make the graph the only stretchable thing
  layout->setRowStretch(1,1);

  if (name)  this->setCaption(name);

  { /* status bar related stuff */
    // for the status bar..
    statusBar = new QStatusBar( this );
    statusBar->setSizeGripEnabled( false );

    /* this _tries_ to add the status bar to the bottom... 
       not elegant design but what he hey?? */
    layout->addMultiCellWidget( statusBar, 
                                layout->numRows(), layout->numRows(),
                                0, layout->numCols()-1 );
    
  /* populate that status bar with them gosh-darned labels and signal/slot
     connections */
    
    currentIndex = new QLabel(CURRENT_INDEX_FORMAT.arg("-"), statusBar);
    mouseOverVector = new QLabel(MOUSE_POS_FORMAT.arg("-").arg("-"),statusBar);
    spikeThreshold = new QLabel(statusBar);       
    lastSpike = new QLabel(LAST_SPIKE_FORMAT.arg("-").arg("-"), statusBar);
    spikeFrequency = new QLabel(SPIKE_FREQUENCY_FORMAT.arg("-").arg("-").arg("-"), 
                                statusBar);
  
    if ( graph->spikeMode() ) {
      setSpikeThresholdStatus(graph->spikeThreshold());
    } else {
      unsetSpikeThresholdStatus();
    }
    
    statusBar->addWidget(currentIndex);
    statusBar->addWidget(mouseOverVector);
    statusBar->addWidget(spikeThreshold);
    statusBar->addWidget(lastSpike);
    statusBar->addWidget(spikeFrequency);
    
    connect(graph, SIGNAL(mouseOverVector(double, uint64)),
            this, SLOT(setMouseVectorStatus(double, uint64)));
    connect(graph, SIGNAL(spikeThresholdSet(double)),
            this, SLOT(setSpikeThresholdStatus(double)));      
    connect(graph, SIGNAL(spikeThresholdUnset()),
            this, SLOT(unsetSpikeThresholdStatus()));
  }

}
  
ECGGraphContainer::~ECGGraphContainer() {
  /* QObject::~QObject() will delete all member/child widgets here */
  if (spikePolarityButtons) {  // this has no real parent
    delete spikePolarityButtons; 
    spikePolarityButtons = 0; 
  }    
}

void
ECGGraphContainer::
consume(const SampleStruct *sample)
{
  graph->plot(sample->data, sample->scan_index);
  detectSpike(sample);
  setCurrentIndexStatus(sample->scan_index);  
}

/** RangeChange slot to be used in conjunction
    with a combobox.  The rangeString should be of the form
    */
void 
ECGGraphContainer::rangeChange( const QString &rangeString ) {
  static QRegExp regexp("-?[0-9]+\\.?-?[0-9]*");
  double newRangeMin, newRangeMax;
  RangeUnit unit;

  if ( parseRangeString(rangeString, newRangeMin, newRangeMax, unit) ) {
    if ( unit == MilliVolts ) {
      /* for notifying anyone interested in range changes on the 
         container-level */
      emit rangeChanged(channelId, findRangeSetting(newRangeMin, newRangeMax, unit));

      newRangeMin /= 1000.0; /* convert the millivolts range settings into 
			      Volts, which is done so that we can always rely
			      on working with the graph in terms of volts */
      newRangeMax /= 1000.0;
    }
    graph->setRange(newRangeMin, newRangeMax);
  }
  
}

void 
ECGGraphContainer::rangeChange( int index ) {
  if (index > -1 && index < rangeComboBox->count())
    rangeComboBox->setCurrentItem( index );
  /* not needed ? */
  rangeChange( rangeComboBox->currentText() );
}

void 
ECGGraphContainer::rangeChange (double newMin, double newMax, RangeUnit u)
{
  int index = findRangeSetting(newMin, newMax, u);
  if ( index >= 0 ) {
    rangeChange( index );
  }
}


/** Returns the index of the new range setting, or -1 if requested
    setting is a dupe.
    Appends a range setting to the combo box, having the specified
    values in the specified units. */
int 
ECGGraphContainer::addRangeSetting (double min, double max, RangeUnit unit) 
{
  static const QString format ("%1%3 - %2%4");

  // first make sure it isn't a dupe
  if (findRangeSetting(min, max, unit) > -1) {
    return -1;
  }
  
  rangeComboBox->insertItem(format.arg(min, 2).arg(max, 2).arg(unitStrings[unit]).arg(unitStrings[unit]));

  return rangeComboBox->count()-1;

}

/* Deletes a range setting from the range combo box at the specified index 
   Returns false if index is invalid/not found */
bool
ECGGraphContainer::deleteRangeSetting (int index) {
  if (index > -1 && index < rangeComboBox->count()) {
    rangeComboBox->removeItem(index);
    return true;
  }  
  return false;
}

int
ECGGraphContainer::findRangeSetting (double min, double max, RangeUnit u) {
  int i;
  double testmin, testmax;
  RangeUnit testunit;

  for (i = 0; i < rangeComboBox->count(); i++) {
    parseRangeString(rangeComboBox->text(i), testmin, testmax, testunit);
    if (testmin == min && testmax == max && testunit == u) {
      return i;
    }    
  }
  return -1;
}

/** slot that is normally triggered from the rangeChanged(double, double)
    signal in the ECGGraph instance bound to this object instance */
void
ECGGraphContainer::updateYAxisLabels(double rangeMin, double rangeMax) {
  static const QString yAxisLabelFormat( "%1 %2" );
  double rangeMiddle = (rangeMin + rangeMax) / 2;
  const QString *unit = &unitStrings[Volts];

  if (fabs(rangeMax) < 1.0) {
    rangeMin *= 1000;
    rangeMiddle *= 1000;
    rangeMax *= 1000;
    unit = &unitStrings[MilliVolts];
  }

  topYLabel->setText( yAxisLabelFormat.arg(rangeMax).arg(*unit) );
  middleYLabel->setText( yAxisLabelFormat.arg(rangeMiddle).arg(*unit) );
  bottomYLabel->setText( yAxisLabelFormat.arg(rangeMin).arg(*unit) );

}

inline int roundit(int n, uint step)
{ return n + ( step - n % step < n % step ? (step - n % step) : -(n % step)); }

void
ECGGraphContainer::setSecondsVisible(int secs)
{
  if (!secs) 
    secondsVisibleBox->setValue(graph->secondsVisible()); // recurse back
  else if (secs % seconds_visible_step)  // round it to step, recurse back
    secondsVisibleBox->setValue(roundit(secs, seconds_visible_step));
  else // propagate it down to the graph since it's a multiple of step
    graph->setSecondsVisible(secs);
}

void
ECGGraphContainer::
detectSpike(const SampleStruct *s)
{
  if (s->spike) {    
    double freq_hz = 1000.0 / (s->spike_period != 0 ? s->spike_period : -1),
           bpm     = freq_hz * 60;
           
    spikeFrequency->setText(SPIKE_FREQUENCY_FORMAT.arg(bpm).arg(freq_hz).arg(s->spike_period));

    /* update last spike label .. */
    lastSpike->setText(LAST_SPIKE_FORMAT.arg(s->data)
                       .arg(static_cast<ulong>(last_spike_index 
                                               = s->scan_index)));    
  }
}

void
ECGGraphContainer::
setCurrentIndexStatus(scan_index_t index)
{
  if (index - last_scan_index >= scan_index_threshold) {
    last_scan_index = index;
    currentIndex->setText(CURRENT_INDEX_FORMAT.arg((ulong) index));
  }
}

/* Slot for updating the 'Mouse pos' status bar line.
   The 'index' we get here (from the ECGGraph class) is inaccurate
   as dropped samples can lead to de-synchronization between
   the graph and the actual sample's scan index.
   In addition we may also have O.B.1 (Obi-Wan) error here -- I didn't 
   check since I will re-write this mechanism soon.  

   TODO: Rethink scan index strategy  --Calin */
void
ECGGraphContainer::
setMouseVectorStatus(double voltage, scan_index_t index)
{   
  mouseOverVector->setText(MOUSE_POS_FORMAT.arg(voltage).arg((long int)index));
}

void
ECGGraphContainer::
setSpikeThresholdStatus(double voltage)
{
  spikeThreshold->setText(SPIKE_THOLD_FORMAT.arg(voltage));
}

void
ECGGraphContainer::
unsetSpikeThresholdStatus()
{
  spikeThreshold->setText(SPIKE_THOLD_FORMAT.arg("-"));
}


void
ECGGraphContainer::mouseUpInGraph()
{
  disconnect (graph,
              SIGNAL(mouseOverAmplitude(double)),
              graph,
              SLOT (setSpikeThreshold(double)));

}

void
ECGGraphContainer::mouseDownInGraph()
{
  connect (graph,
           SIGNAL(mouseOverAmplitude(double)),
           graph,
           SLOT (setSpikeThreshold(double)));
}


void
ECGGraphContainer::
closeEvent (QCloseEvent *e)
{
  emit closing(this); /* This tells daqsystem to remove us from
                         the channel list */
  emit closing(const_cast<const ECGGraphContainer *>(this));
  
  QWidget::closeEvent(e);
}

/** parses the range setting string and places its components in
    the provided reference parameters */
bool
ECGGraphContainer::parseRangeString (const QString & rangeString, 
                                     double & rangeMin, 
                                     double & rangeMax, 
                                     RangeUnit & units) 
{
  static QRegExp regexp("-?[0-9]+\\.?-?[0-9]*");
  int index=0,len=0;
  
  if ( (index = regexp.match(rangeString, 0, &len)) != -1
       && sscanf(rangeString.mid(index,len), "%lf", &rangeMin) 
       && (index = regexp.match(rangeString, index+len, &len)) != -1
       && sscanf(rangeString.mid(index,len), "%lf", &rangeMax) ) {
    // look for the units string
      units = ((rangeString.find(unitStrings[MilliVolts], index + len ) > -1) 
	       ? MilliVolts 
	       : Volts );
      return true;
  }
  return false;
  
}

