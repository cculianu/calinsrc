/*
 * This file is part of the RT-Linux Multichannel Data Acquisition System
 *
 * Copyright (C) 2002 Calin Culianu
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
#ifndef _LAYER_RENDERER_H
#define _LAYER_RENDERER_H

#include <qwidget.h>
#include <qpixmap.h>
#include <qpoint.h>
#include <qsize.h>
#include <qrect.h>
#include <qpainter.h>

#include <map>
#include <vector>


using namespace std;

class LayerGenerator : public QObject 
{
public:  
  virtual ~LayerGenerator(){ };

  virtual void setSize(const QSize & size) { pm.resize(size); generate(); }
  QPixmap & layer()  { return pm; };

  const vector<QRect> & changedAreas() const { return changed_rects; }
  void clearChanges() { changed_rects.clear(); }

  int layerNumber() const { return layer_number; };
  void setLayerNumber(int l) { layer_number = l; }

protected:

  virtual void generate() = 0;

  LayerGenerator( int layer_number, const char * name = 0 ) 
    : QObject(0, name),  layer_number(layer_number)
    { pm.setOptimization(QPixmap::BestOptim); };

  QPixmap pm;
  vector<QRect> changed_rects;
  QPainter painter;

private:
  int layer_number;

};

class LayerRenderer : public QWidget 
{
public:
  LayerRenderer(QWidget * parent = 0, const char * name = 0, WFlags f = 0);
  virtual ~LayerRenderer();
  void render();
  void renderAll();

  /* Reimplemented... from QObject: */
  virtual void insertChild(QObject *);
  virtual void removeChild(QObject *);

protected:
  typedef map<int, LayerGenerator *> Layers_map;
  typedef map<int, LayerGenerator *>::iterator Layers_it;

  QPixmap back_buffer;
  Layers_map layers;

  enum Depths {
    Background = 0,
    Other
  };

  virtual void paintEvent (QPaintEvent *);
  virtual void resizeEvent (QResizeEvent *);
    

private:
  void addLayer(LayerGenerator * g);
  void removeLayer(int layer); 

  vector<QRect> refresh_rects;
  int n_pixels_in_rects;
  void buildRefreshRects();

};

#endif
