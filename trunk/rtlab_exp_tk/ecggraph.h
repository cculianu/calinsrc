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
#ifndef __ECGGRAPH_H
# define __ECGGRAPH_H

#include "common.h"
#include <qwidget.h>
#include <qevent.h>
#include <qcolor.h>

#include <qpainter.h>
#include <qpaintdevice.h>
#include <qpoint.h>
#include <qpointarray.h>
#include <qpixmap.h>

/* To Do: Understand the quirks of qt's line drawing!
   Clean up this code! */
struct ECGPoint {
  ECGPoint() : index(0), amplitude(0) {};
  int64 index; /* x */
  double amplitude; /* y */
};

class ECGGraph : public QWidget {

  Q_OBJECT

 public: 
  
  ECGGraph (int sampleRateHz = 1000, /* the sample rate of the graoh */
	    int secsVisible = 10,    /* number of seconds to fit on the graph 
					 before plotting wraps around to 
					 the beginning */
	    double rangeMin = -1.5,  /* the amplitude of the bottom of graph */
	    double rangeMax = 1.5,   /* the amplitude of the topmost point */
	    QWidget *parent = 0,     /* inherited constructor from QWidget */
	    const char *name = 0,    /* " */
	    WFlags f = 0);           /* " */

  virtual ~ECGGraph();

  /*
    Property Methods 
  */

  /** Use this to change the way the pen looks.  Default is a yellowish
      width=1 pen */
  virtual QPen & plotPen ();  
  
  /** Use this to change the way the spike line pen looks.  
      Default is a reddish dashed-line  width=1 pen */
  virtual QPen & spikeLinePen ();

  /** Use this to change the way the blip pen looks.  
      Note that it will always be filled though
      Default is a  yellowish width=0 pen */
  virtual QPen & blipPen ();  

  /** Change the backgroundColor with this method */
  virtual QColor & backgroundColor ();

  /** Changing gridcolor won't take effect immediately, only after the next
      time the grid needs to be recalculated (usually due to a resize) */
  virtual QColor & gridColor ();

  /** Returns the maximum amplitude this graph can represent */
  virtual double rangeMax() const;

  /** Returns the minimum amplitude this graph can represent */
  virtual double rangeMin() const;

  /** The x-axis property: namely number of seconds visible */
  virtual int secondsVisible() const;

  /** True if we have spike detection turned on */
  virtual bool spikeMode() const;

  /** Returns our spike threshhold.  If spikeMode() is false, this has 
      undefined behavior. */
  virtual double spikeThreshHold() const;

  enum SpikePolarity { Positive = 0, Negative };

  virtual SpikePolarity spikePolarity() const;

  virtual int spikeBlanking() const;

  /*
    Method methods
  */

  /** plots the sample at the next position (keep calling this in a loop).
      X - value for this sample is determined by the current sample pos.
      Y - value is determined by amplitude.  
      FYI is just an arbitrary index that this class can use when
      emitting spikeDetected() */
  virtual void plot (double amplitude, uint64 fyi);
  virtual void plot (double amplitude);

  virtual void paintEvent (QPaintEvent *);
   

signals:
  // to do: move these out of here in favor of
  //        keeping this class generic?

  void rangeChanged(double newRangeMin, double newRangeMax);

  void spikeDetected(uint64 spikeSampleIndex, 
		     double spikeAmplitude);

  void spikeDetected(ECGPoint p);

  /** used to indicate that the mouse is currently at
      a certain Y value (amplitude).  Useful for
      implementing the spike detection control.
      If you want to get this signal whenever the mouse
      pointer is over this graph, see QWidget::setMouseTracking() */
  void mouseOverAmplitude(double amplitude);

  /** emits the cunulative sample index that the mouse is over */
  void mouseOverSampleIndex(uint64 cumSampleIndex);

  /** emits the sample vector that the mouse is over */
  void mouseOverVector(double amplitude, uint64 cumSampleIndex);

  /** emitted when the user clicks on the graph */
  void rightClicked();  void leftClicked(); void eitherClicked();
  void rightReleased(); void leftReleased(); void eitherReleased();

  void secondsVisibleChanged(int seconds);
  void spikeThreshHoldSet(double amplitude);
  void spikeThreshHoldUnset(void);
  void spikePolaritySet(SpikePolarity p);
  void spikeBlankingSet(int sb);

 public slots:
 
  virtual void setRange(double newRangeMin, double newRangeMax);

  /* sets the spike threshhold to value 'amplitude'. */
  virtual void setSpikeThreshHold (double amplitude);

  /* clears the spike threshhold */
  virtual void unsetSpikeThreshHold ();

  /* redraws the graph and resizes its internal data structures to fit
     a new number of seconds setting */
  virtual void setSecondsVisible(int seconds);
  
  virtual void setSpikePolarity(SpikePolarity p);
  virtual void setSpikePolarity(int p) 
    { setSpikePolarity( (p == Positive) ? Positive : Negative); }

  virtual void setSpikeBlanking(int sb);

 protected:

  /** creates real (points array) and virtual (samples array) points,
      for a given amplitude at a given index */
  virtual void makePoint (double amplitude, int sampleIndex = -1);

  virtual void plotLines (int firstIndex, 
			  int lastIndex);

  virtual void resizeEvent (QResizeEvent *event);

  /** used to trap mouse movement and send the mouseOverAmplitude()
      signal in this class */
  virtual void mouseMoveEvent (QMouseEvent *event);
  
  virtual void mousePressEvent (QMouseEvent *event);

  virtual void mouseReleaseEvent (QMouseEvent *event);

  int     numSamples,          // the number of samples this graph supports
          currentSampleIndex,
          sampleRateHz,
          secsVisible;



  double   _rangeMin, _rangeMax,
           *samples;             // array of size numSamples 

  long long  _totalSampleCount; /* tells us how many samples 
					    we have plotted in total */

  QPointArray *points;          /* a cache of all the points written 
                                   (size is numSamples) */  
  QPixmap  _buffer, _background, _threshHoldLine;

  QPainter devicePainter;

  QPen graphPen, spikePen, _blipPen;

  QColor _gridColor, _backgroundColor;

  uint64 outside_concept_of_a_sample_index;

  bool spikeTHoldEnabled;
  double spikeTHold;

  QPoint spikeTHoldPoints[2], lastBlip;

  ECGPoint lastSpike;
  int _spikeBlanking;

  SpikePolarity _spikePolarity;

  /** You need to draw at least 2 line segments with this method, 
      otherwise QPainter::drawPolyline() will be a noop for some 
      strange reason */
  virtual void plotLines (int firstIndex, 
			  int lastIndex, 
			  QPaintDevice *device);

  virtual void deletePlotsBetween (int firstIndex, int secondIndex);


  // initializes/resets _buffer, _background.  Also creates the grid.
  virtual void initBuffer ();


  virtual void initGrid (QPaintDevice & dev, int columns = 10, int rows = 4);


  /** This re-renders the points array from the samples array and
      plots the result on to _buffer
      Usually called from resizeEvent() in order to scale the image to the
      new size before a repaint. */
  virtual void remakeAllPoints ();

  /** The basic formula to translate a sample's vector to 
      physical x and y coordinates relative to the graph.  */
  virtual QPoint sampleVectorToPoint (double amplitude, int sampleIndex);

  /** The inverse of sampleVectorToPoint() */
  virtual void pointToSampleVector (const QPoint & point, 
				    double *amplitude, int *sampleIndex); 

  /** Converts a local sample index to a cumulative sample index */
  virtual long long cumulateSampleIndex(int localSampleIndex);

  virtual void paintSpikeTHoldLine();
  virtual void clearSpikeTHoldLine();
  virtual void computeSpikeTHoldPoints();

  /** draws that little blip artifact at the current position */
  virtual void drawLittleBlip ();

 private:
  void detectSpike(double amplitude);
};



#endif
