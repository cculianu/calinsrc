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
#include "user_cmd.h"
#include "user_to_kernel.h"
#include <asm/bitops.h>

#ifdef __KERNEL__
#  include <linux/comedilib.h>
#  include "rtos_middleman.h"
#  include "rt_process.h"
#  define ERROR(a...) rtos_printf(a)
#else
#  include <unistd.h>
#  include <stdio.h>
#  include <comedilib.h>
#  define ERROR(a...) fprintf(stderr, a)
#endif

static int check_basic_sanity(int count, const struct rtfifo_cmd *cmd);
static void dispatch_command(const struct rtfifo_cmd *cmd, SharedMemStruct *);
static int read_fifo(int fifo, void *buf, unsigned int count);
static int write_fifo(int fifo, const void *buf, unsigned int count);

void do_user_commands(SharedMemStruct *rtp_shm)
{
  int count, fifo = rtp_shm->control_fifo;
  struct rtfifo_cmd cmd;

  if (fifo < 0) return; /* should never happen! */
  
  while ( (count = read_fifo(fifo, &cmd, sizeof(struct rtfifo_cmd))) > 0) 
    {
      if (!check_basic_sanity(count, &cmd)) continue;
      /* todo handle seeking forward to the beginning of a cmd if 
	 we are stuck in the middle of one! */
      dispatch_command(&cmd, rtp_shm);
    }
}


static int check_basic_sanity(int count, const struct rtfifo_cmd *cmd)
{
    int ret = 1;

    if (count != sizeof(struct rtfifo_cmd)) {
      ERROR("user_cmd.c: Error! Count != sizeof(struct rtfifo_cmd)!\n");
      ret = 0; 
    }

    if (cmd->cmd_begin != RTFIFO_BEGIN || cmd->cmd_end != RTFIFO_END) {
      ERROR("user_cmd.c: Error! "
                  "cmd doesn't have the correct required signature!\n");
      ret = 0;       
    }

    if (cmd->struct_version != SHD_SHM_STRUCT_VERSION) {
      ERROR("user_cmd.c: Error! "
                  "cmd doesn't have the correct version!\n");
      ret = 0;             
    }
    
    return ret;
}

static void dispatch_command(const struct rtfifo_cmd *cmd, 
                             SharedMemStruct *rtp_shm)
{
  unsigned int i;
  static const unsigned char reply = 1;
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
  case RTLAB_SET_GAIN:
    CHKCHAN;
    rtp_shm->ai_chan[cmd->chan] = 
      CR_PACK(cmd->chan, cmd->u.gain, CR_AREF(rtp_shm->ai_chan[cmd->chan]));
    break;
  case RTLAB_SET_GAIN_ALL:
    for (i = 0; i < rtp_shm->n_ai_chans; i++)     
      rtp_shm->ai_chan[i] = 
        CR_PACK(i, cmd->u.gain, CR_AREF(rtp_shm->ai_chan[i]));
    break;
  case RTLAB_SET_AREF:
    CHKCHAN;
    rtp_shm->ai_chan[cmd->chan] = 
      CR_PACK(cmd->chan, CR_RANGE(rtp_shm->ai_chan[cmd->chan]), cmd->u.aref);
    break;
  case RTLAB_SET_AREF_ALL:
    for (i = 0; i < rtp_shm->n_ai_chans; i++)     
      rtp_shm->ai_chan[i] = 
        CR_PACK(i, CR_RANGE(rtp_shm->ai_chan[i]), cmd->u.aref);
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
    /* Actually do the command.
       NB: This gets normalized here (and inside rtlab.o's rt-loop) so that
       it's a multiple of 1000Hz or an even factor of 1000Hz 
       ex: 231Hz becomes 250Hz, 1733 Hz becomes 2000Hz, etc.. */
    rtlab_set_sampling_rate(cmd->u.sampling_rate_hz);
    break;
  case RTLAB_SET_SCAN_INDEX:
    ERROR("user_cmd.c: Scan Index change unimplemented!!");
    /*rtp_shm->scan_index = cmd.u->scan_index;*/
    /* TODO: Handle scan index changes!! */
    break;
  default:
    ERROR("user_cmd.c: Unknown user command encountered!\n");
    break;
  }

  /* synchronous reply is always 1 for now... */
  write_fifo(rtp_shm->reply_fifo, (void *)&reply, sizeof(reply));
#undef CHKCHAN
}

#ifdef __KERNEL__
static int read_fifo(int fifo, void *buf, unsigned int count)
{
  return rtos_fifo_get(fifo, buf, count);
}
static int write_fifo(int fifo, const void *buf, unsigned int count)
{
  return rtos_fifo_put(fifo, (void *)buf, count);
}
#else
static int read_fifo(int fifo, void *buf, unsigned int count)
{
  return read(fifo, buf, count);
}
static int write_fifo(int fifo, const void *buf, unsigned int count)
{
  return write(fifo, (void *)buf, count);
}
#endif
