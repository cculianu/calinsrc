/*
 * This file is part of the RT-Linux Multichannel Data Acquisition System
 *
 * Copyright (C) 1999,2000 David Christini
 * Copyright (C) 2001 David Christini, Lyuba Golub
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
#ifndef HELP_TEXT_H
#define HELP_TEXT_H

#include <qstring.h>

const QString copyright_text = 
"DAQ System version " VERSION_NUM "\n
Copyright 1999-2000 David J. Christini
Copyright 2001 David J. Christini, Lyuba Golub

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or (at
your option) any later version.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

A copy of the GNU General Public License can be obtained from the Free
Software Foundation, Inc. at www.gnu.org or 59 Temple Place - Suite
330, Boston, MA 02111-1307, USA.

Limitations of Liability:
In no event shall the initial developers or copyright holders be
liable for any damages whatsoever, including - but not restricted to -
lost revenue or profits or other direct, indirect, special, incidental
or consequential damages, even if they have been advised of the
possibility of such damages, except to the extent invariable law, if
any, provides otherwise.\n"; 

const QString overview_text = "<qt>
This Real-Time Linux based system can be used for acquisition of up to 8 (16 coming soon!) analog voltage signals. The graphical user interface uses the Qt graphical library.
<p align=center><b>DAQ System Components</b></p>
<p><a href=\"#config\">Configuration Window</a>
<p><a href=\"#ascii_bin\">Choosing ASCII/Binary Output</a>
<p><a href=\"#recording\">Recording</a>
<p><a href=\"#channels\">Selecting Which Channel to Configure</a>
<p><a href=\"#pause\">Pausing the Display</a>
<p><a href=\"#spike\">Spike Detection Controls</a>
<p><a href=\"#log\">Saving Text Comments in the Log</a>
<p>
<p><a name=\"config\"></a>A <i>configuration window</i> appears upon program initialization. It allows you to select: (i) the number of channels to display and acquire, (ii) the number of seconds of data to display, and (iii) whether or not to show the spike indicator.
<p><a name=\"ascii_bin\"></a>The <i>ascii/binary buttons</i> allow you to choose whether to record to a binary or ascii file. Binary data requires less disk storage space (but also requires an interpreter program or knowledge of the data structure). Ascii data is space delimited.
<p><a name=\"recording\"></a>Click on the <i>Recording</i> button to start or stop recording. When recording is active the data is being saved to disk. When recording is inactive the data is displayed, but not saved to disk.
<P><a name=\"channels\"></a>Use the <i>channel pulldown menu</i>, located below the ascii/bin buttons, to choose which channel's controls are displayed. You may then modify the properties for that channel. 
<p><a name=\"pause\"></a>Click the <i>pause all</i> button to pause plotting for all channels, or the <i>CH</i> button to pause the selected channel only. Note that only the data display is paused - acquisition and recording (if active) are unaffected by the pause. 
<p><a name=\"log\"></a>The <i>log</i> allows you to conviniently save information about the recording into a text file. Enter text into the editor and click the \"Save\" button to write to a file. Every time this button is clicked the filename.log is rewritten to disk.To enter the current timer value into the log file, place the cursor at the desired position and click \"Time\".

<p>
<p><a name=\"spike\"></a><b>Spike Indicator Controls</b>
<br>Use the <i>threshold scroller</i> adjust spike threshold (horizontal green dotted line). A spike is detected when the voltage crosses this line in the upward direction for positive polarity or downward direction for negative polarity.
<br>The <i>blanking scroller</i> can be used to adjust spike blanking (gray horizontal bar in upper left corner of plot). Spike detection is inactivated for the time duration equivalent to the blanking bar length after each detected spike.
<br>Use the <i>gain pulldown menu</i> to adjust voltage displayed on the y-axis and the scan resolution of the data acquisition board for the active channel. The complete DAQ board resolution (e.g., 12 or 16 bits) is used for the displayed axis range. This does not affect the signal amplification.
<br>The <i>polarity buttons</i> toggle spike polarity.
<br>The spike indicator displays the time interval (in ms) between the two most recently detected spikes (the inter-spike interval).
<p>

</qt>";

const QString config_text= "
   A configuration window appears upon program initialization. It allows you to select: (i) the number of channels to display and acquire, (ii) the number of seconds of data to display, and (iii) whether or not to show the spike indicator."; 

const QString data_type_text = 
   "The ascii/binary buttons allow you to choose whether to record to a binary or ascii file. Binary data requires less disk storage space (but also requires an interpreter program or knowledge of the data structure). Ascii data is space delimited."; 
const QString timer_text = 
   "Time (in seconds) since data acquisition system was started.";
const QString record_text = 
   "Button to start or stop recording. When recording is active the data is being saved to disk. When recording is inactive the data is displayed, but not saved to disk.";
const QString log_text = 
   "Text editor for editing a text log file. The log file name is the same as the selected filename, but with the extension .log (filename.log).";
const QString log_save_text = 
   "Button to save the log file. Every time this button is clicked the filename.log is rewritten to disk.";
const QString log_timer_text = 
   "Button to insert curent timer value at the current cursor position in the log file.";
const QString channel_pulldown_text = 
   "Pulldown menu that allows you to choose which channel's controls are displayed";
const QString pause_button_text = 
   "Button to pause plotting for selected channel. Note that only the data display is paused - acquisition and recording (if active) are unaffected by the pause.";
const QString pause_all_text = 
   "Button to pause plotting for all channels. Note that only the data display is paused - acquisition and recording (if active) are unaffected by the pause.";
const QString pause_group_text = 
   "Buttons to pause plotting for selected channel or all channels. Note that only the data display is paused - acquisition and saving (if active) are unaffected by the pause.";
const QString threshold_scroller_text = 
   "Adjust spike threshold (horizontal green dotted line). A spike is detected when the voltage crosses this line in the upward direction for positive polarity or downward direction for negative polarity.";
const QString blanking_scroller_text = 
   "Adjust spike blanking (gray horizontal bar in upper left corner of plot). Spike detection is inactivated for the time duration equivalent to the blanking bar length after each detected spike.";
const QString gain_pulldown_text = "Adjust voltage displayed on the y-axis and the scan resolution of the data acquisition board for the active channel. The complete DAQ board resolution (e.g., 12 or 16 bits) is used for the displayed axis range. This does not affect the signal amplification.";
const QString polarity_button_text = "Buttons that toggles spike polarity. Spikes are detected when the voltage crosses the green threshold line in the upward direction for positive polarity, and downward direction for negative polarity.";
const QString spike_indicator_text = "Indicates the time interval (in ms) between the two most recently detected spikes (the inter-spike interval).";



#endif
