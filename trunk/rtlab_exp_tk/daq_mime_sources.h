/*
 * This file is part of the RT-Linux Multichannel Data Acquisition System
 *
 * Copyright (C) 1999,2000 David Christini
 * Copyright (c) 2001 David Christini, Lyuba Golub, Calin Culianu
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

#ifndef _DAQ_MIME_SOURCES_H
#define _DAQ_MIME_SOURCES_H
#include <qmime.h>
#include <qimage.h> /* for image () function */
#include "common.h"

namespace DAQMimeSources {
    namespace HTML {
        extern const QString  configWindow,
                              index,
                              about,
                              record,
                              addChannel,
                              pause,
                              spike,
                              print,
                              plugin,
                              log;
    };

    namespace Images {
       extern const QString   addChannel,
                              pause,
                              play,
                              spikeMinus,
                              spikePlus,
                              synch,
                              timeStamp,
                              log,
                              print,
                              plugins,
                              quit,
                              channel, // the graph windows
                              daqSystem, // main icon for application
                              configuration; // icon for config screen
    };

  /* grabs the default, registered factory */
  extern QMimeSourceFactory * factory();
  
  inline QImage image( const QString & img_name ) 
    { return QImage(factory()->data(img_name)->encodedData("image/XPM")); }
};


#endif


