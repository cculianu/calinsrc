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
#include <qmime.h>

#include "daq_help_sources.h"

QString DAQHelpSources::configWindowHelpSource = "confwindow.html",
  DAQHelpSources::mainWindowHelpSource = "main.html",
  DAQHelpSources::recordHelpSource = "record.html",
  DAQHelpSources::addChannelHelpSource = "addchannel.html",
  DAQHelpSources::pauseHelpSource = "pause.html",
  DAQHelpSources::spikeHelpSource = "spike.html",
  DAQHelpSources::logHelpSource = "log.html",
  DAQHelpSources::aboutHelpSource = "about.html",
  DAQHelpSources::pluginHelpSource = "plugin.html",
  DAQHelpSources::printHelpSource = "print.html";

/*--------------------------------------------------------------------------
   Actual html for help
--------------------------------------------------------------------------*/
static const QString 

configHelpText = 
"<qt>A configuration window appears upon program initialization. This window can also be opened by selecting File-Options. Setting changes will take effect next time the application is started.</qt>",

mainHelpText =
  "<qt>"
"This Real-Time Linux based system can be used for acquisition of analog "
"voltage signals. The graphical user interface uses the Qt graphical library."
"<p align=center><b>DAQ System Components</b></p>"
"<p><a href=\"confwindow.html\">Configurating DAQ System</a>"
"<p><a href=\"record.html\">Recording</a>"
"<p><a href=\"addchannel.html\">Adding A Channel</a>"
"<p><a href=\"pause.html\">Pausing the Display</a>"
"<p><a href=\"spike.html\">Spike Detection Controls</a>"
  "<p><a href=\"log.html\">Saving Text Comments in the Log</a>"
  "<p><a href=\"print.html\">Printing</a>"
  "<p><a href=\"plugin.html\">Plugins</a>"
  "<p><a href=\"about.html\">About DAQ System</a></qt>",

aboutHelpText =
  "<qt>"
"This file is part of the RT-Linux Multichannel Data Acquisition System"

"<p> Copyright (C) 1999,2000 David Christini"
"<p> Copyright (c) 2001 David Christini, Lyuba Golub, Calin Culianu"

"<p> This program is free software; you can redistribute it and/or modify it under " 
"the terms of the GNU General Public License as published by the Free Software "
"Foundation; either version 2 of the License, or (at your option) any later "
"version. This program is distributed in the hope that it will be useful, but "
"WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or "
"FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details."
 
"<p>You should have received a copy of the GNU General Public License along with this "
"program (see COPYRIGHT file); if not, write to the Free Software Foundation, Inc., "
"59 Temple Place - Suite 330, Boston, MA 02111-1307, USA, or go to their website at "
  "http://www.gnu.org."
"Limitations of Liability:"
"In no event shall the initial developers or copyright holders be "
"liable for any damages whatsoever, including - but not restricted to - "
"lost revenue or profits or other direct, indirect, special, incidental "
"or consequential damages, even if they have been advised of the "
"possibility of such damages, except to the extent invariable law, if "
"any, provides otherwise.</qt>",

  recordHelpText = "Record",

  addChannelHelpText = "Adding a Channel",

  pauseHelpText = "Pausing the Display",

  spikeHelpText = "Spike Detection Controls",
  
  printHelpText = "Printing",

  pluginHelpText = "Plugins",
  
  logHelpText = "Log";

QMimeSourceFactory * DAQHelpSources::initFactory()
{
  /* register above names and below actual help text with 
     default QMimeSourceFactory here... */
  QMimeSourceFactory *mimeFactory = QMimeSourceFactory::defaultFactory();
  mimeFactory->setText( configWindowHelpSource, configHelpText );
  mimeFactory->setText( mainWindowHelpSource, mainHelpText );
  mimeFactory->setText( aboutHelpSource, aboutHelpText );
  mimeFactory->setText( recordHelpSource, recordHelpText );
  mimeFactory->setText( addChannelHelpSource, addChannelHelpText );
  mimeFactory->setText( pauseHelpSource, pauseHelpText );
  mimeFactory->setText( spikeHelpSource, spikeHelpText );
  mimeFactory->setText( logHelpSource, logHelpText );
  mimeFactory->setText( printHelpSource, printHelpText );
  mimeFactory->setText( pluginHelpSource, pluginHelpText );
  return mimeFactory;
}


/* once all this stuff is filled in.. or at least 1 of the mime sources
   is fully fleshed out and registered.. try to create a window that 
   uses a QTextBrowser and displays your text.. that way you can see
   how all this stuff relates and the results of it all...

   If you get really ambitious, put some menu items in the DAQ System Help menu
   that can call up these help windows.... 
*/
