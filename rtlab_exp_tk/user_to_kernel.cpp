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
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h> 
#include <fcntl.h>
#include <errno.h>
#include <comedilib.h>
#include "user_to_kernel.h"
#include "exception.h"

RTLabKernelNotifier::RTLabKernelNotifier(int control_fifo)
  : fd(0)
{
  QString pathname = "/dev/rtf%1";

  if (control_fifo < 0 
      || (fd = ::open( (pathname = pathname.arg(control_fifo)).latin1(),
		       O_WRONLY | O_SYNC)) < 0 ) {
    throw IllegalStateException("No control FIFO", 
				QString("Could not open rtlab control fifo") +
				(fd < 0 
				 ? QString(". Error was: %1").arg(strerror(errno))
				 : QString("")));
  }
  
}


RTLabKernelNotifier::~RTLabKernelNotifier()
{
  if (fd > -1) {
    ::close(fd);
  }
}

int RTLabKernelNotifier::do_cmd()
{
  ssize_t ret =  ::write(fd, &cmd, sizeof(cmd));
  if (ret < 0) return errno;
  else return 0;
}

int RTLabKernelNotifier::setChan(uint chan, bool on)
{
  cmd.chan = chan;
  cmd.command = RTLAB_SET_CHAN;
  cmd.u.enabled = on;
  return do_cmd();
}

int RTLabKernelNotifier::setAllChans(bool on)
{
  cmd.command = RTLAB_SET_CHAN_ALL;
  cmd.u.enabled = on;
  return do_cmd();
}

int RTLabKernelNotifier::setGain(uint chan, uint gain)
{
  cmd.command = RTLAB_SET_GAIN;
  cmd.chan = chan;
  cmd.u.gain = gain;
  return do_cmd();
}

int RTLabKernelNotifier::setAllGains(uint gain)
{
  cmd.command = RTLAB_SET_GAIN_ALL;
  cmd.u.gain = gain;
  return do_cmd();
}

int RTLabKernelNotifier::setAREF(uint chan, uint aref)
{
  cmd.command = RTLAB_SET_AREF;
  cmd.chan = chan;
  cmd.u.aref = aref;
  return do_cmd();
}

int RTLabKernelNotifier::setAllAREFs(uint aref)
{
  cmd.command = RTLAB_SET_AREF_ALL;
  cmd.u.aref = aref;
  return do_cmd();
}

int RTLabKernelNotifier::setSpike(uint chan, bool on)
{
  cmd.command = RTLAB_SET_SPIKE;
  cmd.chan = chan;
  cmd.u.enabled = on;
  return do_cmd();
}

int RTLabKernelNotifier::setAllSpikes(bool on)
{
  cmd.command = RTLAB_SET_SPIKE_ALL;
  cmd.u.enabled = on;
  return do_cmd();
}

int RTLabKernelNotifier::setSpikePolarity(uint chan, SpikePolarity polarity)
{
  cmd.command = RTLAB_SET_SPIKE_POLARITY;
  cmd.chan = chan;
  cmd.u.polarity = polarity;
  return do_cmd();
}

int RTLabKernelNotifier::setAllSpikePolarities(SpikePolarity polarity)
{
  cmd.command = RTLAB_SET_SPIKE_POLARITY_ALL;
  cmd.u.polarity = polarity;
  return do_cmd();
}

int RTLabKernelNotifier::setSpikeBlanking(uint chan, uint blanking)
{
  cmd.command = RTLAB_SET_SPIKE_BLANKING;
  cmd.chan = chan;
  cmd.u.blanking = blanking;
  return do_cmd();
}

int RTLabKernelNotifier::setAllSpikeBlankings(uint blanking)
{
  cmd.command = RTLAB_SET_SPIKE_BLANKING_ALL;
  cmd.u.blanking = blanking;
  return do_cmd();
}
  
int RTLabKernelNotifier::setSpikeThreshold(uint chan, double thold)
{
  cmd.command = RTLAB_SET_SPIKE_THRESHOLD;
  cmd.chan = chan;
  cmd.u.threshold = thold;
  return do_cmd();
}

int RTLabKernelNotifier::setAllSpikeThresholds(double thold)
{
  cmd.command = RTLAB_SET_SPIKE_THRESHOLD_ALL;
  cmd.u.threshold = thold;
  return do_cmd();
}

int RTLabKernelNotifier::setAttachedPid(int pid)
{
  cmd.command = RTLAB_SET_ATTACHED_PID;
  cmd.u.pid = pid;
  return do_cmd();
}

int RTLabKernelNotifier::unsetAttachedPid() { return setAttachedPid(-1); }

/* sampling rate */
int RTLabKernelNotifier::setSamplingRate(sampling_rate_t rate)
{
  cmd.command = RTLAB_SET_SAMPLING_RATE;
  cmd.u.sampling_rate_hz = rate;
  return do_cmd();
}

/* scan index (dangerous!) */
int RTLabKernelNotifier::setScanIndex(scan_index_t index)
{
  cmd.command = RTLAB_SET_SCAN_INDEX;
  cmd.u.scan_index = index;
  return do_cmd();
}

