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
#include <string>

#include <qmime.h>
#include <qimage.h>

#include "daq_help_sources.h"
#include "daq_images.h"
#include "common.h"

QString DAQHelpSources::configWindowHelpSource = "confwindow.html",
        DAQHelpSources::mainWindowHelpSource   = "main.html",
        DAQHelpSources::recordHelpSource       = "record.html",
        DAQHelpSources::addChannelHelpSource   = "addchannel.html",
        DAQHelpSources::pauseHelpSource        = "pause.html",
        DAQHelpSources::spikeHelpSource        = "spike.html",
        DAQHelpSources::logHelpSource          = "log.html",
        DAQHelpSources::aboutHelpSource        = "about.html",
        DAQHelpSources::pluginHelpSource       = "plugin.html",
        DAQHelpSources::printHelpSource        = "print.html",

        DAQHelpSources::addChannelImage        = "addChannel.img",
        DAQHelpSources::pauseImage             = "pause.img",
        DAQHelpSources::playImage              = "play.img",
        DAQHelpSources::spikeMinusImage        = "spikeMinus.img",
        DAQHelpSources::spikePlusImage         = "spikePlus.img",
        DAQHelpSources::synchImage             = "synch.img",
        DAQHelpSources::timeStampImage         = "timeStamp.img";


/*--------------------------------------------------------------------------
   Actual html for help
--------------------------------------------------------------------------*/

using namespace DAQHelpSources;

static const QString 

configHelpText = 
"<qt>A configuration window appears upon program initialization."
" This window can also be opened by selecting <b>File - Options</b>."
" Keyboard shortcut: <b>Alt + F</b>, then <b>O</b>."
" <p><u>Input Device Selection</u>"
" Select the source for input data. A list of comedi devices will be"
" displayed. The input can also come from a file. "
"<p><u>Output File Selection</u>"
" Then select the name of the file to which data"
" will be saved. Type a file name into the field"
" or press <b>Browse</b> to choose from files on your computer."
" Check the <b>Output Ascii</b> box for ascii output to the file"
" or the <b>Output Binary</b> box for binary output."
" <p><u>Log Template Selection</u>"
" If you would like a template for the log, specify the file name"
" of the template. Type a file name into the field"
" or press <b>Browse</b> to choose from files on your computer."
" The template file you selected will appear in the <b>Log"
" Template Preview</b> window."
"<p>Check <b>Always show this configuration window on"
" application startup</b> box to continue showing the window before"
" running DAQ System."  
"<p>Setting changes will take effect next time the application is started."
"</qt>",

mainHelpText =
  "<qt>"
"This Real-Time Linux based system can be used for acquisition of analog "
"voltage signals. The graphical user interface uses the Qt graphical library."
"<p align=center><b>DAQ System Components</b></p>"
"<p><a href=\"" + configWindowHelpSource + "\">Configurating DAQ System</a>"
"<p><a href=\"" + recordHelpSource + "\">Recording</a>"
"<p><a href=\"" + addChannelHelpSource +"\">Adding A Channel</a>"
"<p><a href=\"" + pauseHelpSource + "\">Pausing the Display</a>"
"<p><a href=\"" + spikeHelpSource + "\">Spike Detection Controls</a>"
  "<p><a href=\"" + logHelpSource + "\">Saving Text Comments in the Log</a>"
  "<p><a href=\"" + printHelpSource + "\">Printing</a>"
  "<p><a href=\"" + pluginHelpSource + "\">Plugins</a>"
  "<p><a href=\"" + aboutHelpSource + "\">About DAQ System</a></qt>",

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

  addChannelHelpText = "<qt><u>Adding a Channel</u><p>"
  "Select <b>Channels - Add Channel</b> or click on the" 
  " <b>Add Channel</b> <br><img src=\"" + addChannelImage + "\"><br> icon in the toolbar. Keyboard shortcut: <b>Ctrl + A</b>. You will be"
  " asked to select the channel to monitor, the voltage"
  " gain for the channel, and the number of seconds to display."
  "</qt>",

  pauseHelpText = "<qt><u>Pausing the Display</u><p>"
  "Press the <b>Pause</b><br><img src=\"" + pauseImage + "\"><br> button to pause the display,"
  " but not the data acquisition. The button will become a play button."
  "<br><img src=\"" + playImage + "\"><br>"
"Press the button again to play."
  "</qt>",

  spikeHelpText = "<qt><u>Spike Detection Controls</u><p>"
  "Press the Postive/Negative button "
"to toggle between"
  " <b>positive</b> <img src=\"" + spikePlusImage + "\"> "
"and <b>negative</b> <img src=\"" + spikeMinusImage + "\"> spike polarity."
  "<p>Click on the graph and drag the red bar to change the spike threshold."
  "</qt>",
  
  printHelpText = "<qt><u>Printing</u><p>"
  "Select <b>File - Print</b> to print the graphs. Choose the graphs to"
  " print by checking the box to the left of each graph."
  "<qt>",

  pluginHelpText = "Plugins",
  
  logHelpText = "<qt><u>Log</u><p>"
  "<u>Opening the Log Window</u>"
  " Select <b>Log - Show Log Window</b> or press <b>Ctrl + L</b>."
  "<p><u>Selecting the log template file</u>"
  " Open the <a href=\"" + configWindowHelpSource + "\">configuration window</a>."
  " Type a file name into the <b>Log Template Selection field</b>"
  " or press <b>Browse</b> to choose from files on your computer."
  " The template file you selected will appear in the <b>Log"
  " Template Preview</b> window."
  "<p><u>Adding a time stamp to the log</u> Press the <b>timestamp</b>"
  " button"
  "<br><img src=\"" + timeStampImage + "\">"
  "<br>on the toolbar or select <b>Log - Insert Timestamp</b>. Keyboard"
  " shortcut: <b>Ctrl + T</b>."
  "</qt>";

QMimeSourceFactory * DAQHelpSources::initFactory()
{
  static bool initialized = false;
  static QMimeSourceFactory *mimeFactory = QMimeSourceFactory::defaultFactory();


  if (!initialized) {

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
      
      using namespace DAQImages;

      mimeFactory->setImage( addChannelImage, plus_img );
      mimeFactory->setImage( pauseImage, pause_img );
      mimeFactory->setImage( playImage, play_img );
      mimeFactory->setImage( spikePlusImage, spike_plus_img );
      mimeFactory->setImage( spikeMinusImage, spike_minus_img );
      mimeFactory->setImage( timeStampImage, timestamp_img );
      mimeFactory->setImage( synchImage, synch_img );
      initialized = true;
  }
  return mimeFactory;
}

