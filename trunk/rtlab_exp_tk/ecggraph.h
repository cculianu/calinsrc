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
#include <vector>

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
  
  ~ECGGraph();

  enum PlotMode {
    Lines,  /* Plots points as connected line segments */
    Points, /* plots points as little points */
    Circles, /* plots points as little circles (slow) */
    Rectangles /* plots points as little rectangles (slow) */
  };

  /*
    Property Methods 
  */

  PlotMode plotMode() const { return pMode; }
  void setPlotMode(PlotMode m) { pMode = m; }

  /* The width of circles, rects, and points... */
  uint pointSize() const { return _pointSize; }
  void setPointSize(uint s) { _pointSize = s; }
  /* The width of the little blip */
  uint blipSize() const { return _blipSize; }
  void setBlipSize(uint s) { _blipSize = s; }
  

  /** Use this to change the way the pen looks.  Default is a yellowish
      width=1 pen */
  QPen & plotPen ();  
  
  /** Use this to change the way the spike line pen looks.  
      Default is a reddish dashed-line  width=1 pen */
  QPen & spikeLinePen ();

  /** Use this to change the way the blip pen looks.  
      Note that it will always be filled though
      Default is a  yellowish width=0 pen */
  QPen & blipPen ();  
  
  /** Change the backgroundColor with this method */
  QColor & backgroundColor ();
  
  /** Changing gridcolor won't take effect immediately, only after the next
      time the grid needs to be recalculated (usually due to a resize) */
  QColor & gridColor ();
  
  /** Returns the maximum amplitude this graph can represent */
  double rangeMax() const;
  
  /** Returns the minimum amplitude this graph can represent */
  double rangeMin() const;

  /** The x-axis property: namely number of seconds visible */
  int secondsVisible() const;
  
  /** The sampling rate this graph thinks we are at */
  int sampleRateHz() const { return _sampleRateHz; };
  
  /** True if we have spike detection turned on */
  bool spikeMode() const;
  
  /** Returns our spike threshhold.  If spikeMode() is false, this has 
      undefined behavior. */
  double spikeThreshold() const;
  
  /*
    Method methods
  */
  
  /* slides everything over and redraws the screen...*/
  void push_back(double amplitude);
  
  /** plots the sample at the next position (keep calling this in a loop).
      X - value for this sample is determined by the current sample pos.
      Y - value is determined by amplitude.  
      FYI is just an arbitrary index that this class can use when
      emitting spikeDetected() */
  void plot (double amplitude, uint64 fyi);
  void plot (double amplitude);
  
  void paintEvent (QPaintEvent *);
  
  /* Renders the entire graph, sans spike line and background color, to
     a pixmap.  The pixmap should already have a width and a height defined.
     
     This method is suitable for printing the graph, as the pixmap
     can be easily printed afterwards. */    
  void renderToPixmap(QPixmap &) const;
  
  uint blockFactor() const { return block_factor; }
  uint setBlockFactor(uint p) { return block_factor = p; }

  /* returns the current sample position from start.  The minimum
     is 0 (after a reset() or on a new graph) and the max is
     samplingRateHz() * secondsVisible() - 1 */
  uint currentPosition() const { return currentSampleIndex; }
  
signals:
  // to do: move these out of here in favor of
  //        keeping this class generic?

  void rangeChanged(double newRangeMin, double newRangeMax);

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
  void spikeThresholdSet(double amplitude);
  void spikeThresholdUnset(void);

  /* emitted whenever the meaning of a particular (vertical) gridline has
     changed */
  void gridlineMeaningChanged(const vector<uint64> &); 

 public slots:
    
  void setRange(double newRangeMin, double newRangeMax);
 
  /* sets the spike threshhold to value 'amplitude'. */
  void setSpikeThreshold (double amplitude);
 
  /* clears the spike threshhold */
  void unsetSpikeThreshold ();

  /* redraws the graph and resizes its internal data structures to fit
     a new number of seconds setting */
  void setSecondsVisible(int seconds);
  
  /* resets this graph to start plotting at the leftmost position */
  void reset(uint64 new_outside_ref_sample_index = 0); 
  
  /* Fast forward the graph's plotting by amt sample indices */
  void ffwd(unsigned int amt); 
   
 protected:
  
  /** creates real (points array) and virtual (samples array) points,
      for a given amplitude at a given index  
      if clear = true, then sets the real point to null and the virtual
      point to a ridiculous value (-10000000) 
  */
   void makePoint (double amplitude, int sampleIndex = -1);

 protected:

   void plotPoints (int firstIndex, 
                           int lastIndex);

  void resizeEvent (QResizeEvent *event);

  /** used to trap mouse movement and send the mouseOverAmplitude()
      signal in this class */
   void mouseMoveEvent (QMouseEvent *event);
  
   void mousePressEvent (QMouseEvent *event);

   void mouseReleaseEvent (QMouseEvent *event);

  unsigned int block_factor; /* actually commit to screen every 
                                block_factor-th sample -- defaults to 10 */
  
  unsigned int _pointSize, _blipSize;

  int     numSamples,          // the number of samples this graph supports
          currentSampleIndex,
          _sampleRateHz,
          secsVisible;

  static const double NOT_VALID_AMPL = -4000000.0; // hopefully will never occur

  double   _rangeMin, _rangeMax,
           *samples;             // array of size numSamples 

  long long  _totalSampleCount; /* tells us how many samples 
                                   we have plotted in total */

  QPointArray *points;          /* a cache of all the points written 
                                   (size is numSamples) */  
  QPixmap  _buffer, _background, _thresholdLine;

  QPainter devicePainter;

  QPen graphPen, spikePen, _blipPen;

  QColor _gridColor, _backgroundColor;

  uint64 outside_concept_of_a_sample_index;
  vector<uint64> sample_indices_at_gridlines; 

  bool spikeTHoldEnabled;
  double spikeTHold;

  QPoint spikeTHoldPoints[2], lastBlip;

  PlotMode pMode;

  /** You need to draw at least 2 line segments with this method, 
      otherwise QPainter::drawPolyline() will be a noop for some 
      strange reason */
   void plotPoints (int firstIndex, int lastIndex, QPaintDevice & device,
                           QPainter & painter, const QPen & pen, 
                           const QPointArray  & points) const;

   void deletePlotsBetween (int firstIndex, int secondIndex);


  // initializes/resets _buffer, _background.  Also creates the grid.
   void initBuffer ();


   void initGrid (QPaintDevice & dev, int width = -1, int height = -1,
                         int columns = 10, int rows = 4) const;


  /** This re-renders the points array from the samples array and
      plots the result on to _buffer
      Usually called from resizeEvent() in order to scale the image to the
      new size before a repaint. */
   void remakeAllPoints ();

  /** The basic formula to translate a sample's vector to 
      physical x and y coordinates relative to the graph.  */
   QPoint sampleVectorToPoint (double amplitude, int sampleIndex,
                               int n_indices = -1,
                               int width = -1, int height = -1) const;

  /** The inverse of sampleVectorToPoint() */
  void pointToSampleVector (const QPoint & point, 
                                    double *amplitude, int *sampleIndex); 

  /** Converts a local sample index to a cumulative sample index */
  long long cumulateSampleIndex(int localSampleIndex);

  void paintSpikeTHoldLine();
  void clearSpikeTHoldLine();
  void computeSpikeTHoldPoints();

  /** draws that little blip artifact at the current position */
  void drawLittleBlip (int index = -1);

  void computeCurrentSampleIndex(bool reset = false);

};



#endif
