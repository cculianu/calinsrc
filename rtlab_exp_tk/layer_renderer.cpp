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
#include <qevent.h>

#include "layer_renderer.h"


LayerRenderer::LayerRenderer(QWidget * parent, const char * name, WFlags f) 
  : QWidget(parent, name, f)
{

}

LayerRenderer::~LayerRenderer() {}

void LayerRenderer::buildRefreshRects()
{

  const int n_buffer_pixels = back_buffer.width() * back_buffer.height();
  bool continue_flg = true;
  Layers_it it;
 
  refresh_rects.clear();
  for (n_pixels_in_rects = 0, it = layers.begin(); 
       continue_flg && it != layers.end(); it++) {
    vector<QRect>::const_iterator rect_it;
    const vector<QRect> & rects = it->second->changedAreas();
    for (rect_it = rects.begin(); rect_it != rects.end(); rect_it ++) {
      n_pixels_in_rects += rect_it->width() * rect_it->height();
      if (n_pixels_in_rects >= n_buffer_pixels) {
        refresh_rects.clear();
        refresh_rects.push_back(QRect(0,0,back_buffer.width(), back_buffer.height()));
        continue_flg = false; 
        break;
      }
      refresh_rects.push_back(*rect_it);
    }
    it->second->clearChanges();
  }
  
}

void LayerRenderer::render()
{
  if (!layers.size()) return;

  
  Layers_it it;
  vector<QRect>::iterator r_it;

  buildRefreshRects();

  for (it = layers.begin(); it != layers.end(); it++) 
    for (r_it = refresh_rects.begin(); r_it != refresh_rects.end(); r_it++) 
      bitBlt(&back_buffer, r_it->topLeft(), 
             &(it->second->layer()), *r_it, CopyROP);
    
  
  
  for (r_it = refresh_rects.begin(); r_it != refresh_rects.end(); r_it++) 
    bitBlt(this, r_it->topLeft(), &back_buffer, *r_it, CopyROP);
    
}

void LayerRenderer::renderAll()
{
  if (!layers.size()) return;

  Layers_it it;

  for (it = layers.begin(); it != layers.end(); it++) 
    bitBlt(&back_buffer, 0,0,
           &(it->second->layer()), 0,0, back_buffer.width(), back_buffer.height(), CopyROP, false);
  bitBlt(this, 0,0,
         &back_buffer, 0,0, back_buffer.width(), back_buffer.height(), CopyROP, true);
  
}

void LayerRenderer::insertChild(QObject * obj)
{
  LayerGenerator * g = dynamic_cast<LayerGenerator *>(obj);
  if (g) addLayer(g);  
  QObject::insertChild(obj);
}

void LayerRenderer::removeChild(QObject * obj)
{
  LayerGenerator * g = dynamic_cast<LayerGenerator *>(obj);
  if (g) removeLayer(g->layerNumber());
  QObject::removeChild(obj);
}

void LayerRenderer::addLayer(LayerGenerator * g) 
{ 
  if (layers.find(g->layerNumber()) != layers.end()) 
    removeChild(layers[g->layerNumber()]);
  
  g->setSize(size());
  layers.insert(Layers_map::value_type(g->layerNumber(), g)); 
}

void LayerRenderer::removeLayer(int layer) 
{ 
  layers.erase(layer); 
}

void LayerRenderer::paintEvent(QPaintEvent *e)
{
  bitBlt(this, e->rect().topLeft(), &back_buffer, e->rect(), CopyROP);
}

void LayerRenderer::resizeEvent(QResizeEvent *e)
{
  Layers_it it;

  for (it = layers.begin(); it != layers.end(); it++) 
    it->second->setSize(e->size());

  back_buffer.resize(e->size());
  
  renderAll();
}



