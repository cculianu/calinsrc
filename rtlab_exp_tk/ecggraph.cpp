#include <qwidget.h>
#include <qevent.h>
#include <qcolor.h>
#include <qregion.h>
#include <qrect.h>
#include <qpainter.h>
#include <qpaintdevice.h>
#include <qpoint.h>
#include <qpointarray.h>
#include <qpixmap.h>
#include <qregexp.h>

#include <math.h>
#include <stdio.h>

#ifdef DEBUG
# include <iostream>
#endif

#include "ecggraph.h"

 
ECGGraph::ECGGraph (int sampleRateHz = 1000,
		    int secsVisible = 10,
		    double rangeMin = -1.5,
		    double rangeMax = 1.5,
		    QWidget *parent = 0, 
		    const char *name = 0, 
		    WFlags f = 0) : 
  QWidget (parent, name, f), 
  numSamples(sampleRateHz * secsVisible),
  currentSampleIndex(0),
  sampleRateHz(sampleRateHz), 
  secsVisible(secsVisible),
  _rangeMin(rangeMin),
  _rangeMax(rangeMax),
  _totalSampleCount(0),
  _gridColor(black),
  _backgroundColor ("#334535") 
{
    
  samples = new double[numSamples];
  points  = new QPointArray (numSamples);
  graphPen.setWidth(1);
  graphPen.setCapStyle(SquareCap);
  graphPen.setColor("#cccc99");   
  spikePen.setWidth(1);
  spikePen.setColor("#ff3333");
  spikePen.setStyle(DashLine);

  initBuffer(); // initialize the pixmap buffer

}

ECGGraph::~ECGGraph() {
  delete (samples);
  delete (points);
}

/** Use this to change the way the pen looks.  Default is a yellowish
    width=1 pen */
QPen & ECGGraph::plotPen () {
  return graphPen;  
}

/** Use this to change the way the spike line pen looks.  
    Default is a reddish dashed-line  width=1 pen */
QPen & ECGGraph::spikeLinePen () {
  return spikePen;  
}
  
/** Change the backgroundColor with this method */
QColor & ECGGraph::backgroundColor () {
  return _backgroundColor; 
}

/** The gridcolor won't take effect immediately, only after the next
    time the grid needs to be recalculated (usually due to a resize) */
QColor & ECGGraph::gridColor () {
  return _gridColor;
}

/** Returns the maximum amplitude this graph can represent */
double ECGGraph::rangeMax () const {
  return _rangeMax;
}

/** Returns the minimum amplitude this graph can represent */
double ECGGraph::rangeMin () const {
  return _rangeMin;
}

/** True if we have spike detection turned on */
bool ECGGraph::spikeMode() const {
  return spikeTHoldEnabled;
}

/** Returns our spike threshhold.  If spikeMode() is false, this has 
    undefined behavior. */
double ECGGraph::spikeThreshHold() const {
  return spikeTHold;
}

// rangeChange slot
void ECGGraph::setRange(double newRangeMin, double newRangeMax) {
  _rangeMin = newRangeMin;
  _rangeMax = newRangeMax;
  remakeAllPoints();
  // now, repaint everything
  QPaintEvent pe ( QRegion ( 0, 0, width(), height()) );
  paintEvent(&pe);
  emit rangeChanged(_rangeMin, _rangeMax);
}

// plots a sample at the next position
void ECGGraph::plot (double amplitude) {

  int plotFactor = 10; // actually draw to screen every plotFactor-th sample

  if (currentSampleIndex && currentSampleIndex != _totalSampleCount
      && !(currentSampleIndex  % plotFactor)) {
    deletePlotsBetween (currentSampleIndex - plotFactor, currentSampleIndex);
  }

  makePoint(amplitude);

  if (currentSampleIndex && !(currentSampleIndex % plotFactor) )
    plotLines (currentSampleIndex - plotFactor, currentSampleIndex);  

  if (spikeTHoldEnabled && amplitude >= spikeTHold) {
    emit spikeDetected(this, _totalSampleCount, amplitude);
  }

  currentSampleIndex = ++currentSampleIndex % numSamples;
  _totalSampleCount++;  
}

void ECGGraph::paintEvent (QPaintEvent *paintEvent) {
  const QRect &rect = paintEvent->rect();
  bitBlt(this, rect.topLeft(), &_buffer, rect);
  paintSpikeTHoldLine();

}

/* 
   protected:
*/

/** creates real (points array) and virtual (samples array) points,
    for a given amplitude at a given index */
void ECGGraph::makePoint (double amplitude, int sampleIndex = -1) {
  QPoint point;
  
  if (sampleIndex < 0) 
    sampleIndex = currentSampleIndex;
  
  point = sampleVectorToPoint(amplitude, sampleIndex);

#ifdef DEBUG
  //cout << "created Point(" << point.x() << ", " << point.y() << ") for amp. " << amplitude << " and index " << sampleIndex << endl;
#endif
  points->setPoint(sampleIndex, point); // save the physical point
  samples[sampleIndex] = amplitude;     // save the abstract point's amplitude
}


void ECGGraph::plotLines (int firstIndex, int lastIndex) {
  // paint to both memory (_buffer) and to screen using private method
  plotLines(firstIndex, lastIndex, &_buffer);
  plotLines(firstIndex, lastIndex, this);
  
}

void ECGGraph::resizeEvent (QResizeEvent *) {
  remakeAllPoints();
  clearSpikeTHoldLine();
  computeSpikeTHoldPoints();
}

void ECGGraph::mouseMoveEvent (QMouseEvent *event) {
  double amplitude;
  int sampleIndex, x = event->x(), y = event->y();

  if (x >= 0 && x <= width() && y >=0 && y <= height() ) {
    
    pointToSampleVector (event->pos(), &amplitude, &sampleIndex);
    long long cumI = cumulateSampleIndex(sampleIndex); /* grab the cumulative 
							  sample index */
#ifdef DEBUG  
    //    cout << name() << "::mouse is at (" << x << ", " << y << ") "
    //	 << "which is sample vector (" << amplitude << ", " << cumI << ") (a,si)"  << endl;
#endif
    emit mouseOverAmplitude(amplitude);
    emit mouseOverSampleIndex(cumI);
    emit mouseOverVector(amplitude, cumI);
  }
}

void ECGGraph::mousePressEvent(QMouseEvent *event) {
  if (event->button() == RightButton) {
    emit rightClicked();
  }

  if (event->button() == LeftButton) {
    emit leftClicked();
  }
}

/*
  private:
*/

  /** You need to draw at least 2 line segments with this method, 
      otherwise QPainter::drawPolyline() will be a noop for some 
      strange reason */
void ECGGraph::plotLines (int firstIndex, 
			  int lastIndex, 
			  QPaintDevice *device) {
  devicePainter.begin (device);
  devicePainter.setPen(graphPen);
  devicePainter.drawPolyline(*points, firstIndex, lastIndex-firstIndex+1);
  devicePainter.end();
}

void ECGGraph::deletePlotsBetween (int firstIndex, int secondIndex) {
  QPoint &p1 = points->at(firstIndex), &p2 = points->at(secondIndex);
  
  bitBlt(this, p1.x()+1, 0, &_background, p1.x()+1, 0, p2.x()-p1.x(), height(), CopyROP, TRUE);
  bitBlt(&_buffer, p1.x()+1, 0, &_background, p1.x()+1, 0, p2.x()-p1.x(), height(), CopyROP, TRUE);

  // now, repaint our spike line
  if (spikeTHoldEnabled) {
    bitBlt(this, p1.x()+1, spikeTHoldPoints[0].y(), &_threshHoldLine, p1.x()+1, 0, p2.x()-p1.x(), spikePen.width(), CopyROP, TRUE);
  }
  
}


  // initializes/resets _buffer, _background.  Also creates the grid.
void ECGGraph::initBuffer () {
  _buffer.resize(width(), height());
  _buffer.fill(_backgroundColor); 
  
  initGrid (_buffer, secsVisible);
  _background = _buffer; // save the initial buffer as the background
  this->setBackgroundPixmap (_background); // set the background of the graph 

  // add-on for the threshHoldLine buffer...
  _threshHoldLine.resize(width(), spikePen.width());
}


void ECGGraph::initGrid (QPaintDevice & dev, int columns = 10, int rows = 4) {
  QPainter painter(&dev);
  QPen pen(_gridColor, 1, DotLine);

  painter.setPen(pen);
  int i, w = width(), h = height();
  for (i = 0; i < w; i+=w/columns) {
    painter.drawLine(i,0,i,h);
  }
  for (i = 0; i < h; i+=h/rows) {
    painter.drawLine(0,i,w,i);
  }
  painter.end();

}


/** This re-renders the points array from the samples array and
    plots the result on to _buffer
    Usually called from resizeEvent() in order to scale the image to the
    new size before a repaint. */
void ECGGraph::remakeAllPoints () {
  uint i, maxIndex;

  computeSpikeTHoldPoints (); /* re-compute the spike threshold */
  initBuffer(); /* reset the background pixmap so that 
		   we may procees with a fresh render */
  maxIndex = ( _totalSampleCount < numSamples 
	       ? _totalSampleCount 
	       : numSamples );

  for (i = 0; i < maxIndex ; i++) {      
    makePoint(samples[i],i); /* re-calculate points */
  }
  if (i) {
    plotLines(0,i-1);
  }
}

QPoint ECGGraph::sampleVectorToPoint (double amplitude, int sampleIndex) {
  int w = width(), h = height(), x, y; 

  x =  (int)rint(( (w+0.0) / numSamples ) * sampleIndex);
  y =  (int)rint(( h / (_rangeMax - _rangeMin) ) * (_rangeMax - amplitude));

  return QPoint(x, y);
}

void ECGGraph::pointToSampleVector(const QPoint &point, double *amplitude, int *sampleIndex) {
  int w = width(), h = height(), x = point.x(), y = point.y(); 

  *sampleIndex = (int)rint( x / ( (w+0.0) / numSamples ) );
  *amplitude = 0 - ( (y / (h / (_rangeMax - _rangeMin))) - _rangeMax);
}

long long ECGGraph::cumulateSampleIndex(int s) {
  if (s > currentSampleIndex) {
    // wrap around..
    return (_totalSampleCount-1 - currentSampleIndex) - (numSamples-1 - s);
  }
  return _totalSampleCount-1 - (currentSampleIndex - s);
}

void ECGGraph::computeSpikeTHoldPoints () {
  spikeTHoldPoints[0] = sampleVectorToPoint(spikeTHold, 0);
  spikeTHoldPoints[0].setX(0);
  spikeTHoldPoints[1].setX(width());
  spikeTHoldPoints[1].setY(spikeTHoldPoints[0].y());
}

void ECGGraph::clearSpikeTHoldLine () {
  static QPoint one (1,1);

  if (spikeTHoldPoints[0].isNull() && spikeTHoldPoints[1].isNull())
    return;

  QRect rect(spikeTHoldPoints[0]-one, spikeTHoldPoints[1]+one);
  bitBlt(this, spikeTHoldPoints[0]-one, &_buffer, rect);
  // clear the back buffer for the line
  bitBlt(&_threshHoldLine, 0, 0, &_background, 0, spikeTHoldPoints[0].y(), _threshHoldLine.width(), spikePen.width());
}

void ECGGraph::paintSpikeTHoldLine () {
  
  if (spikeTHoldEnabled) {
    devicePainter.begin(this);
    devicePainter.setPen(spikePen);
    devicePainter.drawLine(spikeTHoldPoints[0], spikeTHoldPoints[1]);
    devicePainter.end();
    // now duplicate for the backbuffer for this line
    devicePainter.begin(&_threshHoldLine);
    devicePainter.setPen(spikePen);
    devicePainter.drawLine(QPoint(0,0), QPoint(spikeTHoldPoints[1].x(),0));
    devicePainter.end();
  }
}

void ECGGraph::setSpikeThreshHold(double amplitude) {
  if (spikeTHoldEnabled) {
    clearSpikeTHoldLine();
  }
  spikeTHoldEnabled = true;
  spikeTHold = amplitude;
  computeSpikeTHoldPoints();
  paintSpikeTHoldLine();
}

void ECGGraph::unsetSpikeThreshHold() {
  spikeTHoldEnabled = false;
  clearSpikeTHoldLine();
  spikeTHoldPoints[0] = spikeTHoldPoints[1] = QPoint (0,0);
}

