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

  Areas::const_iterator area_it;
  Layers_it it;
 
  refresh_area = QRegion();
  for (n_pixels_in_rects = 0, it = layers.begin();  it != layers.end(); it++) {
    const Areas & areas = (*it)->changedAreas();
    for (area_it = areas.begin(); area_it != areas.end(); area_it++) {
      refresh_area += *area_it; // unite() the Areas (QRegions)
    }
    (*it)->clearChanges(); 
  }
  
}

void LayerRenderer::render()
{
  if (!layers.size() || back_buffer.isNull()) return;

  
  Layers_it it;

  buildRefreshRects();

  if (refresh_area.isNull()) return;

  QRect bounds = refresh_area.boundingRect();

  for (it = layers.begin(); it != layers.end(); it++) 
    bitBlt(&back_buffer, bounds.topLeft(), 
           &((*it)->layer()), bounds, CopyROP);        
}

void LayerRenderer::renderAll()
{
  if (!layers.size() || back_buffer.isNull() ) return;

  Layers_it it;

  for (it = layers.begin(); it != layers.end(); it++) {
    bitBlt(&back_buffer, 0,0,
           &((*it)->layer()), 0,0, back_buffer.width(), back_buffer.height(), 
           CopyROP);
    (*it)->clearChanges();
  }
  refresh_area = QRect(0, 0, back_buffer.width(), back_buffer.height());
  
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
  /* add the layer in order of its layer value */
  Layers_it i;
  for (i = layers.begin(); i != layers.end(); i++) {
    if (g->layerNumber() < (*i)->layerNumber()) {
      i = layers.insert(i, g);
      break;
    }
  }
  if (i == layers.end()) layers.push_back(g);
  
  g->setSize(size());
  renderAll();
  refresh();
}

void LayerRenderer::removeLayer(int layer) 
{ 
  Layers_it i;

  for (i = layers.begin(); i != layers.end(); i++) {
    if ((*i)->layerNumber() == layer)
      i = layers.erase(i); 
  }
  renderAll();
  refresh();
}

void LayerRenderer::paintEvent(QPaintEvent *e)
{
  render();
  refresh();
  bitBlt(this, e->rect().topLeft(), &back_buffer, e->rect(), CopyROP);
}

void LayerRenderer::resizeEvent(QResizeEvent *e)
{
  Layers_it it;

  for (it = layers.begin(); it != layers.end(); it++) 
    (*it)->setSize(e->size());

  back_buffer.resize(e->size());
  
  renderAll();
  refresh();
}

void LayerRenderer::refresh()
{
  if (refresh_area.isNull()) return;
  QRect bounds = refresh_area.boundingRect();
  bitBlt(this, bounds.topLeft(), &back_buffer, bounds, CopyROP);
}



