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
/*
  user_to_kernel.h 
  -------------------
  Defines all the shared parameters and structures  that are used for 
  user to kernel communication  in the DAQ System
*/
#ifndef _USER_TO_KERNEL_H
#define _USER_TO_KERNEL_H 1

#include "shared_stuff.h"

#ifdef __cplusplus
extern "C" {
#endif


  /*
    TODO: Implement ability to affect AO subdevice from userland? 
  */

typedef enum {
  RTLAB_SET_CHAN = 0,
  RTLAB_SET_CHAN_ALL,
  RTLAB_SET_CHANSPEC,
  RTLAB_SET_CHANSPEC_ALL,
  RTLAB_SET_SPIKE,
  RTLAB_SET_SPIKE_ALL,
  RTLAB_SET_SPIKE_POLARITY,
  RTLAB_SET_SPIKE_POLARITY_ALL,
  RTLAB_SET_SPIKE_BLANKING,
  RTLAB_SET_SPIKE_BLANKING_ALL,
  RTLAB_SET_SPIKE_THRESHOLD,
  RTLAB_SET_SPIKE_THRESHOLD_ALL,
  RTLAB_SET_ATTACHED_PID,
  RTLAB_SET_SAMPLING_RATE,
  RTLAB_SET_SCAN_INDEX
} rtlab_user_cmd;

#define RTFIFO_BEGIN (0xfade)
#define RTFIFO_END   (0xedaf)

#define RTFIFO_CMD_INIT \
{ \
  struct_version : SHD_SHM_STRUCT_VERSION, \
  cmd_begin : RTFIFO_BEGIN, \
  cmd_end : RTFIFO_END \
}

#define RTFIFO_CMD_CLR(x) \
do { \
  x->struct_version = SHD_SHM_STRUCT_VERSION;\
  x->cmd_begin = RTFIFO_BEGIN; \
  x->cmd_end = RTFIFO_END; \
} while(0)


struct rtfifo_cmd {
#ifdef __cplusplus
  rtfifo_cmd() { RTFIFO_CMD_CLR(this); }
#endif
  int cmd_begin;
  int struct_version; /* Always equals SHD_SHM_STRUCT_VERSION */

  rtlab_user_cmd command;
  unsigned int chan; /* Not used for every command */
  union {
    unsigned int chanspec; /* for RTLAB_SET_CHANSPEC */
    SpikePolarity polarity; /* for RTLAB_SET_SPIKE_POLARITY */
    char enabled;  /* for RTLAB_SET_CHAN and RTLAB_SET_SPIKE */
    unsigned int blanking; /* for RTLAB_SET_SPIKE_BLANKING */
    double threshold; /* for RTLAB_SET_SPIKE_THRESHOLD */
    sampling_rate_t sampling_rate_hz; /* for RTLAB_SET_SAMPLING_RATE */
    scan_index_t    scan_index; /* for RTLAB_SET_SCAN_INDEX */
    int pid; /* for RTLAB_SET_ATTACHED_PID */
  } u;
  int cmd_end;
};

#ifdef __cplusplus
}

class RTLabKernelNotifier
{
 public:
   RTLabKernelNotifier(int control_fifo);
  ~RTLabKernelNotifier();

  /* channel on/off */
  int setChan(uint chan, bool on);
  int setAllChans(bool on);

  /* chanspecs (gain, aref) */
  int setChanspec(uint chanspec);
  int setChanspec(uint chan, uint range, uint aref);
  int setAllChanspecs(uint chanspec);
  int setAllChanspecs(uint range, uint aref);
  
  /* spike on/off */
  int setSpike(uint chan, bool on);
  int setAllSpikes(bool on);
  
  /* spike polarity */
  int setSpikePolarity(uint chan, SpikePolarity polarity);
  int setAllSpikePolarities(SpikePolarity polarity);

  /* spike blanking */
  int setSpikeBlanking(uint chan, uint blanking);
  int setAllSpikeBlankings(uint blanking); 
  
  /* spike threshold */
  int setSpikeThreshold(uint chan, double thold);
  int setAllSpikeThresholds(double thold);

  /* attached pid */
  int setAttachedPid(int pid);
  int unsetAttachedPid();

  /* sampling rate */
  int setSamplingRate(sampling_rate_t rate);

  /* scan index (dangerous!) */
  int setScanIndex(scan_index_t index);

 private:
  rtfifo_cmd cmd;
  int fd;

  int do_cmd();
};
#endif


#endif
