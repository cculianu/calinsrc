#include <qframe.h>
#include <qlayout.h>
#include <qvbox.h>
#include <qhbox.h>
#include <qlabel.h>
#include <qevent.h>
#include <qregexp.h>
#include <qtooltip.h>
#include <stdio.h>

#include "ecggraphcontainer.h"

/* used for string parsing and building in the combo box
   see also enum RangeUnit */
const QString ECGGraphContainer::unitStrings[3] =  { "V", "mV" };

  /** Pass in a graph to become the container of.  Graph gets reparented
      to be a child of this class! */
ECGGraphContainer::ECGGraphContainer(ECGGraph *graph, 			     
				     QWidget *parent = 0, 
				     const char *name = 0, 
				     WFlags flags = 0) 
  : 
    QFrame (parent, name, flags), 
    graph(graph)
    
{  
  static const QString yAxisLabelFormat( "%1 V" );
  
  this->setFrameStyle(StyledPanel | Raised);
  
  // this is the master layout for this widget
  layout = new QGridLayout (this, 2, 2, 1, 1);
  

  // the controls on top
  controlsBox = new QHBox(this);
  controlsBox->setFrameStyle(StyledPanel | Raised);
  controlsBox->setMargin(2);
  controlsBox->setSpacing(2);

  // graph's name in the top left
  graphNameLabel = new QLabel((name ? name : ""), controlsBox);
  graphNameLabel->setAlignment(AlignRight | AlignTop);  
  graphNameLabel->setMargin(1);
  graphNameLabel->setFrameStyle(QFrame::StyledPanel | QFrame::Raised);

  // the range settings control
  QLabel *tmpLabel = new QLabel ("Change Scale: ", controlsBox);  
  rangeComboBox = new QComboBox(controlsBox, 
				QString("%1 Scale Box").arg(graph->name()));
  QToolTip::add(rangeComboBox, "Use this to change graph scale (Y Axis Range).");
  QToolTip::add(tmpLabel, "Use this to change graph scale (Y Axis Range).");

  controlsBox->setStretchFactor(rangeComboBox, 1);  
  layout->addMultiCellWidget (controlsBox, 0, 0, 0, 1);

  connect(rangeComboBox,  
	  SIGNAL(activated(const QString &)),
	  this, 
	  SLOT (rangeChange(const QString &))); 

  // add the current range setting (from the graph) to the combo box
  addRangeSetting(graph->rangeMin(), graph->rangeMax(), Volts);

  // the number of seconds visible spin box up top
  tmpLabel = 
    new QLabel("Secs. Visible", controlsBox); 
  /* we can do the above because of qt's 'quasi-garbage collection'  */
  secondsVisibleBox = new QSpinBox(0, 100000, 1, controlsBox);
  secondsVisibleBox->setValue(graph->secondsVisible());
  QToolTip::add(tmpLabel, "Use this to change the number of seconds visible "
		          "in your graph (X Axis Scale).");
  QToolTip::add(secondsVisibleBox, "Use this to change the number of seconds "
		                   "visible in your graph (X Axis Scale).");
  connect(secondsVisibleBox, SIGNAL(valueChanged ( int )),
	  graph, SLOT(setSecondsVisible(int)));
  

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
  // so that labels are always truly reflecting what the graph is doing...
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
	   SLOT(unsetSpikeThreshHold(void)));

  graph->reparent(this, QPoint(0,0) );

  // re-synch the labels to the graph, just to be sure...
  // setRange implicitly emits rangeChanged()
  graph->setRange(graph->rangeMin(), graph->rangeMax());
  QToolTip::add(graph, "Click+Drag to set a spike threshhold.");
  
  layout->addWidget (labelBox, 1, 0);
  layout->addWidget (graph, 1, 1);

  layout->setColStretch(1,1); // make the graph the only stretchable thing
  layout->setRowStretch(1,1);

}
  
ECGGraphContainer::~ECGGraphContainer() {
  /* QObject::~QObject() will delete all member/child widgets here */
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
      emit rangeChanged ( findRangeSetting(newRangeMin, newRangeMax, unit) );

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

void
ECGGraphContainer::mouseUpInGraph()
{
  disconnect (graph,
	      SIGNAL(mouseOverAmplitude(double)),
	      graph,
	      SLOT (setSpikeThreshHold(double)));

}

void
ECGGraphContainer::mouseDownInGraph()
{
  connect (graph,
	   SIGNAL(mouseOverAmplitude(double)),
	   graph,
	   SLOT (setSpikeThreshHold(double)));
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

