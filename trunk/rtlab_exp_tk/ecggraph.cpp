/*
 * This file is part of the RT-Linux Multichannel Data Acquisition System
 *
 * Copyright (C) 2001 Calin Culianu
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
#include <string.h>

#ifdef DEBUG
# include <iostream>
#endif

#include "ecggraph.h"

 
ECGGraph::ECGGraph (int sampleRateHz,
                    int secsVisible,
                    double rangeMin,
                    double rangeMax,
                    QWidget *parent, 
                    const char *name, 
                    WFlags f) : 
  QWidget (parent, name, f | WRepaintNoErase | WResizeNoErase), 
  plotFactor(10),
  numSamples(sampleRateHz * secsVisible),
  _sampleRateHz(sampleRateHz), 
  secsVisible(secsVisible),
  _rangeMin(rangeMin),
  _rangeMax(rangeMax),
  samples(0),
  _totalSampleCount(0),    
  _gridColor(black),
  _backgroundColor ("#334535"),
  outside_concept_of_a_sample_index(0),
  spikeTHoldEnabled(false)
{
  samples = new double[numSamples];
  points  = new QPointArray (numSamples);
  graphPen.setWidth(1);
  graphPen.setCapStyle(SquareCap);
  graphPen.setColor("#cccc99");   
  spikePen.setWidth(1);
  spikePen.setColor("#ff3333");
  spikePen.setStyle(DashLine);
  _blipPen.setWidth(0);
  _blipPen.setCapStyle(RoundCap);
  _blipPen.setColor("#ffffcc");

  initBuffer(); // initialize the pixmap buffer

  computeCurrentSampleIndex(true); /* resets the current sample index */
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

/** Use this to change the way the blip pen looks.  
    Note that it will always be filled though
    Default is a  yellowish width=0 pen */
QPen & 
ECGGraph::blipPen () {
  return _blipPen;
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

/** The x-axis property: namely number of seconds visible */
int ECGGraph::secondsVisible() const 
{
  return secsVisible;
}

/** True if we have spike detection turned on */
bool ECGGraph::spikeMode() const {
  return spikeTHoldEnabled;
}

/** Returns our spike threshhold.  If spikeMode() is false, this has 
    undefined behavior. */
double ECGGraph::spikeThreshold() const {
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

void ECGGraph::push_back(double amplitude)
{
  static const int plotFactor = 125; /* actually draw to screen every 
                                       plotFactor-th sample */
  bool update_to_screen_flg = !(_totalSampleCount  % plotFactor);
  int i;

  if (update_to_screen_flg) deletePlotsBetween (0, currentSampleIndex);
  
  for (i = currentSampleIndex; i > 0; i--) {
    makePoint(samples[i-1], i); // slide everything over 1
    samples[i] = samples[i-1];
  }
  
  makePoint(amplitude, 0);

  if (update_to_screen_flg) {

    /* plot the lines */
    plotLines (0, currentSampleIndex);  

    /* now draw the little blip */
    drawLittleBlip(0);

  }

  if (currentSampleIndex < (numSamples - 1)) ++currentSampleIndex;

  _totalSampleCount++;  
  
}

// plots a sample at the next position
void ECGGraph::plot (double amplitude, uint64 sample_index) {
  outside_concept_of_a_sample_index = sample_index;  
  plot(amplitude);
}

void ECGGraph::plot (double amplitude) {

  if (currentSampleIndex && currentSampleIndex != _totalSampleCount
      && !(currentSampleIndex  % plotFactor)) {
    deletePlotsBetween (currentSampleIndex - plotFactor, currentSampleIndex);
  }

  makePoint(amplitude);

  if (currentSampleIndex && !(currentSampleIndex % plotFactor) ) {

    /* plot the lines */
    plotLines (currentSampleIndex - plotFactor, currentSampleIndex);  

    /* now draw the little blip */
    drawLittleBlip();

  }
  
  _totalSampleCount++;  
  computeCurrentSampleIndex();
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
  
  if (sampleIndex < 0) 
    sampleIndex = currentSampleIndex;
  
  QPoint point ( sampleVectorToPoint(amplitude, sampleIndex) );

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
}

void ECGGraph::mouseMoveEvent (QMouseEvent *event) {
  double amplitude;
  int sampleIndex, x = event->x(), y = event->y();

  if (x >= 0 && x <= width() && y >=0 && y <= height() ) {
    
    pointToSampleVector (event->pos(), &amplitude, &sampleIndex);
    /* grab the cumulative sample index */
    uint64 cumI = (uint64)cumulateSampleIndex(sampleIndex); 
#ifdef DEBUG  
    //    cout << name() << "::mouse is at (" << x << ", " << y << ") "
    //	 << "which is sample vector (" << amplitude << ", " << cumI << ") (a,si)"  << endl;
#endif
    emit mouseOverAmplitude(amplitude);
    emit mouseOverSampleIndex(cumI);
    emit mouseOverVector(amplitude, cumI);
  }
}

void ECGGraph::mousePressEvent(QMouseEvent *event) 
{

  if (event->button() == RightButton) {
    emit rightClicked();
  }

  if (event->button() == LeftButton) {
    emit leftClicked();
  }
  emit eitherClicked();

  QWidget::mousePressEvent(event); // probably a useless call ...
}

void ECGGraph::mouseReleaseEvent(QMouseEvent *event)
{
  if (event->button() == RightButton) {
    emit rightReleased();
  }

  if (event->button() == LeftButton) {
    emit leftReleased();
  }

  emit eitherReleased();

  QWidget::mouseReleaseEvent(event); // probably a useless call ...
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
    bitBlt(this, p1.x()+1, spikeTHoldPoints[0].y(), &_thresholdLine, p1.x()+1, 0, p2.x()-p1.x(), spikePen.width(), CopyROP, TRUE);
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
  _thresholdLine.resize(width(), spikePen.width());
}


void ECGGraph::initGrid (QPaintDevice & dev, int columns = 10, int rows = 4) {
  QPainter painter(&dev);
  QPen pen(_gridColor, 1, DotLine);

  painter.setPen(pen);
  float i;
  int w = width(), h = height();

  for (i = 0.0; columns && i < w; i += w/(float)columns) {
    painter.drawLine((int)i,0,(int)i,h);
  }
  for (i = 0.0; rows && i < h; i += h/(float)rows) {
    painter.drawLine(0,(int)i,w,(int)i);
  }
  painter.end();

}


/** This re-renders the points array from the samples array and
    plots the result on to _buffer
    Usually called from resizeEvent() in order to scale the image to the
    new size before a repaint. */
void ECGGraph::remakeAllPoints () {
  int i, maxIndex;

  computeSpikeTHoldPoints (); /* re-compute the spike threshold */
  initBuffer(); /* reset the background pixmap so that 
                   we may proceed with a fresh render */
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
  bitBlt(&_thresholdLine, 0, 0, &_background, 0, spikeTHoldPoints[0].y(), _thresholdLine.width(), spikePen.width());
}

void ECGGraph::paintSpikeTHoldLine () {
  
  if (spikeTHoldEnabled) {
    devicePainter.begin(this);
    devicePainter.setPen(spikePen);
    devicePainter.drawLine(spikeTHoldPoints[0], spikeTHoldPoints[1]);
    devicePainter.end();
    // now duplicate for the backbuffer for this line
    devicePainter.begin(&_thresholdLine);
    devicePainter.setPen(spikePen);
    devicePainter.drawLine(QPoint(0,0), QPoint(spikeTHoldPoints[1].x(),0));
    devicePainter.end();
  }
}


void ECGGraph::setSpikeThreshold(double amplitude) 
{
  if (spikeTHoldEnabled) {
    clearSpikeTHoldLine();
  }
  spikeTHoldEnabled = true;
  spikeTHold = amplitude;
  computeSpikeTHoldPoints();
  paintSpikeTHoldLine();
  emit spikeThresholdSet(amplitude);  
}

void ECGGraph::unsetSpikeThreshold() {
  spikeTHoldEnabled = false;
  clearSpikeTHoldLine();
  spikeTHoldPoints[0] = spikeTHoldPoints[1] = QPoint (0,0);
  emit spikeThresholdUnset();
}

void ECGGraph::setSecondsVisible(int seconds)
{
  if (seconds <= 0) return;

  int oldNumSamples = numSamples;
  secsVisible = seconds;
  numSamples = secsVisible * _sampleRateHz;

  // resize the samples array
  double *newSamples = new double[numSamples];
  if (numSamples > oldNumSamples) {
    memcpy(newSamples, samples, oldNumSamples * sizeof(double));
    double filler = newSamples[oldNumSamples - 1];
    for (int i = oldNumSamples; i < numSamples; i++) newSamples[i] = filler;
  } else {
    memcpy(newSamples, samples, numSamples * sizeof(double));
  }
  delete (samples);
  samples = newSamples;

  // resize the points array
  points->resize(numSamples);

  // do some extra sanity bookkeeping
  if (currentSampleIndex >= numSamples)
    currentSampleIndex = numSamples - 1;

  // now redraw the graph with the new grid and x-axis scale
  reset(); /* hack to keep our gridline labels correct */
  //remakeAllPoints();
  emit secondsVisibleChanged(seconds);
}

void
ECGGraph::reset() 
{
  computeCurrentSampleIndex(true);
  _totalSampleCount = 0;
  remakeAllPoints();
}

void ECGGraph::drawLittleBlip (int index) {
  static const int blipsize = 4; /* in pixels */

  if (index < 0) index = currentSampleIndex;

  QPoint here (points->at(index));
  here.setY(here.y()-blipsize/2); // center  
  here.setX(here.x()-blipsize/2); //        it
  
  

  if (!lastBlip.isNull()) {
    /* erase old blip */
    bitBlt(this, lastBlip.x(), lastBlip.y(), &_buffer, lastBlip.x(), lastBlip.y(), blipsize, blipsize, CopyROP, TRUE);      
    /* todo: fix this so the spike threshhold line doesn't get clobbered */

  }
  devicePainter.begin (this);
  BrushStyle oldBrush = devicePainter.brush().style();
  QColor oldBrushColor = devicePainter.brush().color();
  devicePainter.setBrush(SolidPattern);
  devicePainter.setBrush(_blipPen.color());
  devicePainter.setPen(_blipPen);
  devicePainter.drawEllipse(here.x(), here.y(), blipsize, blipsize);
  devicePainter.setBrush(oldBrushColor);
  devicePainter.setBrush(oldBrush);
  devicePainter.end();
  lastBlip = here;
}

void ECGGraph::computeCurrentSampleIndex(bool reset = false)
{
  currentSampleIndex = ( reset ? 0 : ++currentSampleIndex % numSamples);

  if (reset) {
    sample_indices_at_gridlines.clear();
    for (int i = 0; i < secsVisible; i++) 
      sample_indices_at_gridlines.push_back(outside_concept_of_a_sample_index + i * (numSamples / secsVisible));
  }

  /* this is always true if reset is true */
  if (!(currentSampleIndex % (numSamples / secsVisible)) 
      && currentSampleIndex >= 0) {     
      sample_indices_at_gridlines[static_cast<uint>((currentSampleIndex / (float)numSamples) * secsVisible)] 
        = outside_concept_of_a_sample_index;
      emit gridlineMeaningChanged(sample_indices_at_gridlines);
  }
}

