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
#include "daq_images.h"

/* Icons */
#include "add_channel.xpm"
#include "pause.xpm"
#include "play.xpm"
#include "spike_minus.xpm"
#include "spike_plus.xpm"
#include "synch.xpm"
#include "timestamp.xpm"
#include "log.xpm"
#include "print.xpm"
#include "plugins.xpm"
#include "daq_system.xpm"
#include "configuration.xpm"
#include "quit.xpm"
#include "channel.xpm"

const QImage   DAQImages::add_channel_img( add_channel_xpm ),
               DAQImages::pause_img( pause_xpm ),
               DAQImages::play_img( play_xpm ),
               DAQImages::spike_minus_img( spike_minus_xpm ),
               DAQImages::spike_plus_img( spike_plus_xpm ),
               DAQImages::synch_img( synch_xpm ),
               DAQImages::timestamp_img( timestamp_xpm ),
               DAQImages::log_img( log_xpm ),
               DAQImages::print_img( print_xpm ),
               DAQImages::plugins_img( plugins_xpm ),
               DAQImages::daq_system_img( daq_system_xpm ),
               DAQImages::configuration_img( configuration_xpm ),
               DAQImages::quit_img( quit_xpm ),
               DAQImages::channel_img( channel_xpm);
