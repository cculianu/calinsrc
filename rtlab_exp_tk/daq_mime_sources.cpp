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
#include <qimage.h>

#include "daq_mime_sources.h"
#include "daq_images.h"
#include "common.h"

static QMimeSourceFactory * initFactory();


const QString 
    DAQMimeSources::HTML::configWindow = "confwindow.html",
    DAQMimeSources::HTML::index        = "index.html",
    DAQMimeSources::HTML::record       = "record.html",
    DAQMimeSources::HTML::addChannel   = "addchannel.html",
    DAQMimeSources::HTML::pause        = "pause.html",
    DAQMimeSources::HTML::spike        = "spike.html",
    DAQMimeSources::HTML::log          = "log.html",
    DAQMimeSources::HTML::about        = "about.html",
    DAQMimeSources::HTML::plugin       = "plugin.html",
    DAQMimeSources::HTML::print        = "print.html",

    DAQMimeSources::Images::addChannel        = "addChannel.img",
    DAQMimeSources::Images::pause             = "pause.img",
    DAQMimeSources::Images::play              = "play.img",
    DAQMimeSources::Images::spikeMinus        = "spikeMinus.img",
    DAQMimeSources::Images::spikePlus         = "spikePlus.img",
    DAQMimeSources::Images::synch             = "synch.img",
    DAQMimeSources::Images::timeStamp         = "timeStamp.img";


/*--------------------------------------------------------------------------
   Actual html for help
--------------------------------------------------------------------------*/

using namespace DAQMimeSources;
using namespace DAQMimeSources::HTML;

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

indexHelpText =
  "<qt>"
"This Real-Time Linux based system can be used for acquisition of analog "
"voltage signals. The graphical user interface uses the Qt graphical library."
"<p align=center><b>DAQ System Components</b></p>"
"<p><a href=\"" + configWindow       + "\">Configurating DAQ System</a>"
"<p><a href=\"" + record       + "\">Recording</a>"
"<p><a href=\"" + addChannel       +"\">Adding A Channel</a>"
"<p><a href=\"" + pause       + "\">Pausing the Display</a>"
"<p><a href=\"" + spike       + "\">Spike Detection Controls</a>"
  "<p><a href=\"" + log       + "\">Saving Text Comments in the Log</a>"
  "<p><a href=\"" + print       + "\">Printing</a>"
  "<p><a href=\"" + plugin       + "\">Plugins</a>"
  "<p><a href=\"" + about       + "\">About DAQ System</a></qt>",

aboutHelpText =
  "<qt>"
"This program is part of the RT-Linux Multichannel Data Acquisition System"
" (http://www.rtlab.org) "
"<p> Copyright (C) 1999,2000 David Christini"
"<p> Copyright (c) 2001 Calin Culianu, Lyuba Golub, David Christini"

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
  " <b>Add Channel</b> <br><img src=\"" + DAQMimeSources::Images::addChannel + "\"><br> icon in the toolbar. Keyboard shortcut: <b>Ctrl + A</b>. You will be"
  " asked to select the channel to monitor, the voltage"
  " gain for the channel, and the number of seconds to display."
  "</qt>",

  pauseHelpText = "<qt><u>Pausing the Display</u><p>"
  "Press the <b>Pause</b><br><img src=\"" + DAQMimeSources::Images::pause + "\"><br> button to pause the display,"
  " but not the data acquisition. The button will become a play button."
  "<br><img src=\"" + DAQMimeSources::Images::play + "\"><br>"
"Press the button again to play."
  "</qt>",

  spikeHelpText = "<qt><u>Spike Detection Controls</u><p>"
  "Press the Postive/Negative button "
"to toggle between"
  " <b>positive</b> <img src=\"" + DAQMimeSources::Images::spikePlus + "\"> "
"and <b>negative</b> <img src=\"" + DAQMimeSources::Images::spikeMinus + "\"> spike polarity."
  "<p>Click on the graph and drag the red bar to change the spike threshold."
  "</qt>",
  
  printHelpText = "<qt><u>Printing</u><p>"
  "Select <b>File - Print</b> to print the graphs. Choose the graphs to"
  " print by checking the box to the left of each graph."
  "<qt>",

  pluginHelpText = "<qt><u>Plugins</u></qt><p>"
"<u>Description</u> - "
"Plugins are custom pieces of software that are designed to fit in to this "
"application seamlessly.  Their purpose is to modify or enhance the behavior "
"of the DAQ System program.  Plugins may be written by following the example "
"set by the <a href=\"#avnstim\"><code>avn_stim.so</code></a> plugin."
"<P>"
"<dl><dt><a name=\"avnstim\"><B>avn_stim.so</b></a></dt>"
"    <dd>avn_stim is a plugin written by Calin Culianu and David Christini "
"        of Cornell Medical College's Cardiac Electrodynamics department."
"        It comes in two pieces: the kernel module avn_stim.o which isl"
"        designed to run at realtime priority, and avn_stim.so, which"
"        is the .so file loaded by the daq_system user application.</dd>"

,
  
  logHelpText = "<qt><u>Log</u><p>"
  "<u>Opening the Log Window</u>"
  " Select <b>Log - Show Log Window</b> or press <b>Ctrl + L</b>."
  "<p><u>Selecting the log template file</u>"
  " Open the <a href=\"" + configWindow       + "\">configuration window</a>."
  " Type a file name into the <b>Log Template Selection field</b>"
  " or press <b>Browse</b> to choose from files on your computer."
  " The template file you selected will appear in the <b>Log"
  " Template Preview</b> window."
  "<p><u>Adding a time stamp to the log</u> Press the <b>timestamp</b>"
  " button"
  "<br><img src=\"" + DAQMimeSources::Images::timeStamp + "\">"
  "<br>on the toolbar or select <b>Log - Insert Timestamp</b>. Keyboard"
  " shortcut: <b>Ctrl + T</b>."
  "</qt>";

static QMimeSourceFactory * initFactory()
{
  static QMimeSourceFactory *mimeFactory = 0;

  if (!mimeFactory) {

      mimeFactory = QMimeSourceFactory::defaultFactory();
      mimeFactory->setText( configWindow, configHelpText );
      mimeFactory->setText( DAQMimeSources::HTML::index, indexHelpText );
      mimeFactory->setText( about, aboutHelpText );
      mimeFactory->setText( record, recordHelpText );
      mimeFactory->setText( addChannel, addChannelHelpText );
      mimeFactory->setText( pause, pauseHelpText );
      mimeFactory->setText( spike, spikeHelpText );
      mimeFactory->setText( log, logHelpText );
      mimeFactory->setText( print, printHelpText );
      mimeFactory->setText( plugin, pluginHelpText );
      
      using namespace DAQImages;

      mimeFactory->setImage( DAQMimeSources::Images::addChannel, plus_img );
      mimeFactory->setImage( DAQMimeSources::Images::pause, pause_img );
      mimeFactory->setImage( DAQMimeSources::Images::play, play_img );
      mimeFactory->setImage( DAQMimeSources::Images::spikePlus, spike_plus_img );
      mimeFactory->setImage( DAQMimeSources::Images::spikeMinus, spike_minus_img );
      mimeFactory->setImage( DAQMimeSources::Images::timeStamp, timestamp_img );
      mimeFactory->setImage( DAQMimeSources::Images::synch, synch_img );

  }

  return mimeFactory;
}

QMimeSourceFactory * DAQMimeSources::factory() { return initFactory(); }

