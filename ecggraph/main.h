#include <qobject.h>
#include <qtimer.h>
#include "ecggraph.h"

class Dummy: public QObject {

  Q_OBJECT
  
public:
  Dummy(ECGGraph *graphs[]);
  ECGGraph **graphs;
  int blanking;
  
public slots:
    
  void fullPlot ();
  void realPlot();
  void spikeDetected(const ECGGraph *graph, 
		     long long index, double amplitude);

private:

  double d;
  int x, i, limit;
  long long lastSpike;
  QTimer *timer;
  

};
