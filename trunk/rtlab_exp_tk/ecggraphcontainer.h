#ifndef __ECGGRAPHCONTAINER_H
# define __ECGGRAPHCONTAINER_H

#include <qframe.h>
#include <qlayout.h>
#include <qvbox.h>
#include <qhbox.h>
#include <qlabel.h>
#include <qevent.h>
#include <qcombobox.h>
#include <qspinbox.h>
#include "ecggraph.h"


class ECGGraphContainer : public QFrame {
  Q_OBJECT
public:

  /** Pass in a graph to become the container of.  Graph gets reparented
      to be a child of this class! */
  ECGGraphContainer(ECGGraph *graph, 
		    QWidget *parent = 0, 
		    const char *name = 0, 
		    WFlags flags = 0);
  
  virtual ~ECGGraphContainer();

  ECGGraph *graph;

  enum RangeUnit { /* units used in addRangeSetting */
    Volts,
    MilliVolts
  };
  /** this populates the combobox with a string of the right
      format and the correct units */
  int addRangeSetting(double min, double max, RangeUnit unit);

  /** Deletes a range setting from the range combo box at the specified index 
      Returns false if index is invalid/not found */
  bool deleteRangeSetting(int index);

  /** returns index of the specified range setting in the combo box control, 
      or -1 if the setting is not found in the range combo box */
  int findRangeSetting(double rangeMin, double rangeMax, RangeUnit unit);

  /** an array of strings to translate Volts and MilliVolts constants
      into pretty strings for display: 
      Ex. usage: QString volts = ECGGraphContainer::unitStrings[Volts]; */
  static const QString unitStrings[];

 signals:
  void rangeChanged( int newIndex );

public slots:
  /** Custom slot...
      For use with combo-boxes that contain descriptive text of the new ranges.
      The rangeString should have the form "*xx.xx*yy.yy*" where 
      xx.xx and yy.yy are string representations of the floats for min
      and max ranges, respectively.
  */
  void rangeChange( const QString &rangeString ); 
  void rangeChange( int index );
  void rangeChange(double rangeMin, double rangeMax, RangeUnit unit);

  /** slot that is normally triggered from the rangeChanged(double, double)
      signal in the ECGGraph instance bound to this object instance */
  void updateYAxisLabels(double rangeMin, double rangeMax);


private:

  bool parseRangeString(const QString &string, double *rangeMin, double *rangeMax, RangeUnit *units);
 
  QGridLayout *layout;

  QLabel *topYLabel, *middleYLabel, *bottomYLabel, *graphNameLabel;

  QVBox *labelBox; // for the y axis labels
 
  QHBox *controlsBox; // for the controls above the graph

  QComboBox *rangeComboBox;  // holds range labels
  QSpinBox *secondsVisibleBox; // holds/changes the number of seconds visible

};

#endif
