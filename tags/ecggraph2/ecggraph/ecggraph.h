#ifndef __ECGGRAPH_H
# define __ECGGRAPH_H

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

  /*
    Property Methods 
  */

  /** Use this to change the way the pen looks.  Default is a yellowish
      width=1 pen */
  QPen & plotPen ();  
  
  /** Use this to change the way the spike line pen looks.  
      Default is a reddish dashed-line  width=1 pen */
  QPen & spikeLinePen ();

  /** Change the backgroundColor with this method */
  QColor & backgroundColor ();

  /** Changing gridcolor won't take effect immediately, only after the next
      time the grid needs to be recalculated (usually due to a resize) */
  QColor & gridColor ();

  /** Returns the maximum amplitude this graph can represent */
  double rangeMax() const;

  /** Returns the minimum amplitude this graph can represent */
  double rangeMin() const;

  /** True if we have spike detection turned on */
  bool spikeMode() const;

  /** Returns our spike threshhold.  If spikeMode() is false, this has 
      undefined behavior. */
  double spikeThreshHold() const;

  /*
    Method methods
  */

  /** plots the sample at the next position (keep calling this in a loop).
      X - value for this sample is determined by the current sample pos.
      Y - value is determined by amplitude.  */
  void plot (double amplitude);

  void paintEvent (QPaintEvent *);
   

signals:
  // to do: move these out of here in favor of
  //        keeping this class generic?
  void rangeChanged(double newRangeMin, double newRangeMax);

  void spikeDetected(const ECGGraph *graph, 
		     long long spikeSampleNumber, double spikeAmplitude);

  /** used to indicate that the mouse is currently at
      a certain Y value (amplitude).  Useful for
      implementing the spike detection control.
      If you want to get this signal whenever the mouse
      pointer is over this graph, see QWidget::setMouseTracking() */
  void mouseOverAmplitude(double amplitude);

  /** emits the cunulative sample index that the mouse is over */
  void mouseOverSampleIndex(long long cumSampleIndex);

  /** emits the sample vector that the mouse is over */
  void mouseOverVector(double amplitude, long long cumSampleIndex);

  /** emitted when the user clicks on the graph */
  void rightClicked(); void leftClicked();

public slots:
 
 void setRange(double newRangeMin, double newRangeMax);

 void setSpikeThreshHold (double amplitude);
 void unsetSpikeThreshHold ();

 protected:

  /** creates real (points array) and virtual (samples array) points,
      for a given amplitude at a given index */
  void makePoint (double amplitude, int sampleIndex = -1);

  void plotLines (int firstIndex, 
		  int lastIndex);

  void resizeEvent (QResizeEvent *event);

  /** used to trap mouse movement and send the mouseOverAmplitude()
      signal in this class */
  void mouseMoveEvent (QMouseEvent *event);
  
  void mousePressEvent (QMouseEvent *event);
  
 private:
  
  int      numSamples,          // the number of samples this graph supports
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
 
  QPen graphPen, spikePen;

  QColor _gridColor, _backgroundColor;

  bool spikeTHoldEnabled;
  double spikeTHold;
  QPoint spikeTHoldPoints[2];

  /** You need to draw at least 2 line segments with this method, 
      otherwise QPainter::drawPolyline() will be a noop for some 
      strange reason */
  void plotLines (int firstIndex, 
		  int lastIndex, 
		  QPaintDevice *device);

  void deletePlotsBetween (int firstIndex, int secondIndex);


  // initializes/resets _buffer, _background.  Also creates the grid.
  void initBuffer ();


  void initGrid (QPaintDevice & dev, int columns = 10, int rows = 4);


  /** This re-renders the points array from the samples array and
      plots the result on to _buffer
      Usually called from resizeEvent() in order to scale the image to the
      new size before a repaint. */
  void remakeAllPoints ();

  /** The basic formula to translate a sample's vector to 
      physical x and y coordinates relative to the graph.  */
  QPoint sampleVectorToPoint (double amplitude, int sampleIndex);

  /** The inverse of sampleVectorToPoint() */
  void pointToSampleVector (const QPoint & point, double *amplitude, int *sampleIndex); 

  /** Converts a local sample index to a cumulative sample index */
  long long cumulateSampleIndex(int localSampleIndex);

  void paintSpikeTHoldLine();
  void clearSpikeTHoldLine();
  void computeSpikeTHoldPoints();
};


#endif
