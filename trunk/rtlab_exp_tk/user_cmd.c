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
  user_cmd.c -- implements rtlab.o's control fifo functionality 
*/
#include "user_to_kernel.h"
#include "rt_process.h"
#include "rtos_middleman.h"
#include "user_cmd.h"
#include <asm/bitops.h>

static int check_basic_sanity(int count, const struct rtfifo_cmd *cmd);
static void dispatch_command(const struct rtfifo_cmd *cmd);

void do_user_commands(void)
{
  int count;
  struct rtfifo_cmd cmd;

  if (rtp_shm->control_fifo < 0) return; /* should never happen! */
  
  while ( (count = rtos_fifo_get(rtp_shm->control_fifo, 
				 &cmd, sizeof(struct rtfifo_cmd))) > 0) 
    {
      if (!check_basic_sanity(count, &cmd)) continue;
      /* todo handle seeking forward to the beginning of a cmd if 
	 we are stuck in the middle of one! */
      dispatch_command(&cmd);
    }
}


static int check_basic_sanity(int count, const struct rtfifo_cmd *cmd)
{
    int ret = 1;

    if (count != sizeof(struct rtfifo_cmd)) {
      rtos_printf("user_cmd.c: Error! Count != sizeof(struct rtfifo_cmd)!\n");
      ret = 0; 
    }

    if (cmd->cmd_begin != RTFIFO_BEGIN || cmd->cmd_end != RTFIFO_END) {
      rtos_printf("user_cmd.c: Error! "
		  "cmd doesn't have the correct required signature!\n");
      ret = 0;       
    }

    if (cmd->struct_version != SHD_SHM_STRUCT_VERSION) {
      rtos_printf("user_cmd.c: Error! "
		  "cmd doesn't have the correct version!\n");
      ret = 0;             
    }
    
    return ret;
}

static void dispatch_command(const struct rtfifo_cmd *cmd)
{
  unsigned int i;
#define CHKCHAN if (cmd->chan >= rtp_shm->n_ai_chans) break
  switch(cmd->command) {
  case RTLAB_SET_CHAN:
    CHKCHAN;
    if (cmd->u.enabled)
      set_bit(cmd->chan, rtp_shm->ai_chans_in_use);
    else
      clear_bit(cmd->chan, rtp_shm->ai_chans_in_use);
    break;
  case RTLAB_SET_CHAN_ALL:
    for (i = 0; i < rtp_shm->n_ai_chans; i++) 
      if (cmd->u.enabled)
	set_bit(i, rtp_shm->ai_chans_in_use);
      else
	clear_bit(i, rtp_shm->ai_chans_in_use);
    break;
  case RTLAB_SET_CHANSPEC:
    CHKCHAN;
    rtp_shm->ai_chan[cmd->chan] = CR_PACK(cmd->chan, 
					  CR_RANGE(cmd->u.chanspec),
					  CR_AREF(cmd->u.chanspec));
    break;
  case RTLAB_SET_CHANSPEC_ALL:
    for (i = 0; i < rtp_shm->n_ai_chans; i++) 
      rtp_shm->ai_chan[i] = CR_PACK(i, 
				    CR_RANGE(cmd->u.chanspec),
				    CR_AREF(cmd->u.chanspec));
    break;
  case RTLAB_SET_SPIKE:
    CHKCHAN;
    if (cmd->u.enabled)
      set_bit(cmd->chan, rtp_shm->spike_params.enabled_mask);
    else
      clear_bit(cmd->chan, rtp_shm->spike_params.enabled_mask);      
    break;
  case RTLAB_SET_SPIKE_ALL:
    for (i = 0; i < rtp_shm->n_ai_chans; i++) 
      if (cmd->u.enabled)
	set_bit(i, rtp_shm->spike_params.enabled_mask);
      else
	clear_bit(i, rtp_shm->spike_params.enabled_mask);
    break;
  case RTLAB_SET_SPIKE_POLARITY:
    CHKCHAN;
    if (cmd->u.polarity)
      set_bit(cmd->chan, rtp_shm->spike_params.polarity_mask);
    else
      clear_bit(cmd->chan, rtp_shm->spike_params.polarity_mask);          
    break;
  case RTLAB_SET_SPIKE_POLARITY_ALL:
    for (i = 0; i < rtp_shm->n_ai_chans; i++)
      if (cmd->u.polarity)
	set_bit(i, rtp_shm->spike_params.polarity_mask);
      else
	clear_bit(i, rtp_shm->spike_params.polarity_mask);
    break;
  case RTLAB_SET_SPIKE_BLANKING:
    CHKCHAN;
    rtp_shm->spike_params.blanking[cmd->chan] = cmd->u.blanking;
    break;
  case RTLAB_SET_SPIKE_BLANKING_ALL:
    for (i = 0; i < rtp_shm->n_ai_chans; i++)
      rtp_shm->spike_params.blanking[i] = cmd->u.blanking;
    break;
  case RTLAB_SET_SPIKE_THRESHOLD:
    CHKCHAN;
    rtp_shm->spike_params.threshold[cmd->chan] = cmd->u.threshold;
    break;
  case RTLAB_SET_SPIKE_THRESHOLD_ALL:
    for (i = 0; i < rtp_shm->n_ai_chans; i++)
      rtp_shm->spike_params.threshold[i] = cmd->u.threshold;
    break;
  case RTLAB_SET_ATTACHED_PID:
    rtp_shm->attached_pid = ( cmd->u.pid <= 0 ? 0 : cmd->u.pid );
    break;
  case RTLAB_SET_SAMPLING_RATE:
    rtp_shm->sampling_rate_hz = cmd->u.sampling_rate_hz;
    /* NB: This gets re-adjusted later inside rtlab.o's rt-loop so that
       it's a multiple of 500Hz or so */
    break;
  case RTLAB_SET_SCAN_INDEX:
    rtos_printf("user_cmd.c: Scan Index changs unimplemented!!");
    /*rtp_shm->scan_index = cmd.u->scan_index;*/
    /* TODO: Handle scan index changes!! */
    break;
  default:
    rtos_printf("user_cmd.c: Unknown user command encountered!\n");
    break;
  }
#undef CHKCHAN
}
