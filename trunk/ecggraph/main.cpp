#include <qapplication.h>
#include <qlayout.h>
#include <qlabel.h>
#include <qwidget.h>
#include <qframe.h>
#include <qvbox.h>
#include <qpoint.h>
#include <qcombobox.h>
#include <qpushbutton.h>
#include <qtimer.h>

#include <iostream>
#include <math.h>
#include <stdio.h>
#include "ecggraph.h"
#include "ecggraphcontainer.h"
#include "main.h"


Dummy::Dummy(ECGGraph *graphs[]) : graphs(graphs), blanking(100), lastSpike(0)  {
  timer = new QTimer(this);
  
  ECGGraph **graph = graphs;
  while (*graph) {
    connect(*graph, SIGNAL(spikeDetected(const ECGGraph *, long long, double)), SLOT(spikeDetected(const ECGGraph *, long long, double)));
    graph++;
  }
}

void Dummy::fullPlot () {
  limit = 100000;
  d = 0, i = 0, x = 0;
  this->connect(timer, SIGNAL(timeout(void)), SLOT(realPlot(void)));
  timer->start(1);
}


void Dummy::realPlot() {
  static const int lump = 30; // how many we do everytime we are called
  

  if (i > limit) {
    timer->stop();
    return;
  }

  ECGGraph **graph;
  int initial = i;

  while (i < initial+lump && i < limit) {
    for (graph = graphs; *graph; graph++) {
      d=sin(x/200.40)*7 + cos(x/120.12);      
      (*graph)->plot(d);
      x/=2;
    }
    x = ++i;
  }
}

void Dummy::spikeDetected(const ECGGraph *graph, 
			  long long index, double amplitude) {

  if (!lastSpike || lastSpike+blanking < index) {
    cout << "Spike detected from '" << graph->name() << "' at index " << 
      index << " with amplitude " << amplitude << " (spike amplitude is " << graph->spikeThreshHold() << ") " << endl;    
  }
  lastSpike = index;
}


int
main (int argc, char **argv) {
  const int numGraphs = 3;
  int i;

  QApplication a (argc, argv);

  QVBox vbox;
  ECGGraph *graphs[numGraphs+1]; 
  ECGGraphContainer *graphcontainers[numGraphs+1];


  for (i = 0; i < numGraphs; i++) {
    graphs[i] = new ECGGraph(1000, 40, -10.1, 10.1, 0, QString("Graph %1").arg(i));
    graphcontainers[i] = new ECGGraphContainer (graphs[i], &vbox, QString("Container %1").arg(i));

    graphcontainers[i]->addRangeSetting(-2.0, 2.0, ECGGraphContainer::Volts);
    graphcontainers[i]->addRangeSetting(-25, 25, ECGGraphContainer::Volts); 
    graphcontainers[i]->addRangeSetting(-500,500, ECGGraphContainer::MilliVolts);
    // now test if dupe suppression works..
    graphcontainers[i]->addRangeSetting(-2.0, 2.0, ECGGraphContainer::Volts);
    graphcontainers[i]->addRangeSetting(-25, 25, ECGGraphContainer::Volts); 
    graphcontainers[i]->addRangeSetting(-500,500, ECGGraphContainer::MilliVolts);

  }
  graphs[i] = 0; graphcontainers[i] = 0; // null out end of this list

  Dummy d(graphs);
  QPushButton bPlot("Plot...", &vbox), bQuit("Quit", &vbox);

  d.connect(&bPlot, SIGNAL(clicked(void)), SLOT(fullPlot(void)));
  a.connect(&bQuit, SIGNAL(clicked(void)), SLOT(quit(void)));

  a.setMainWidget(&vbox);
  vbox.show();
  
  return a.exec();
}



