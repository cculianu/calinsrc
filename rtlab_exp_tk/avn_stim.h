/*! AVN Stim - The kernel side defs.. */

/*
 * This file is part of the RT-Linux Multichannel Data Acquisition System
 *
 * Copyright (C) 1999,2000 David Christini
 * Copyright (C) 2001,2002 Calin Culianu
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
#ifndef _AVN_STIM_H
#  define _AVN_STIM_H

#include "shared_stuff.h"

#ifdef __cplusplus
extern "C" {
#endif


/* A struct for use in the user-process-bound fifo, called Liebnitz since this
   is akin to the 'derivative' of a function.. namely a momentary analysis of
   state */
struct AVNLiebnitz {
  int rr_interval;
  int stimuli;
  double g_val;
  scan_index_t scan_index;
};

#ifndef __cplusplus
  typedef struct AVNLiebnitz AVNLiebnitz;
#endif

#define AVN_G_ADJ_AUTOMATIC 0x0
#define AVN_G_ADJ_MANUAL 0x1

struct AVNShared {
  AVNLiebnitz latest_snapshot;
  int rr_target; /* in milliseconds */
  int stim_on; /* if nonzero, do the actual stimulations */
  int g_adjustment_mode;
#ifdef __KERNEL__
  int in_fifo_minor; /* dirty hack to make 'in' mean incoming from userland */
  int out_fifo_minor;
#else
  int out_fifo_minor; /* dirty hack to make 'out' mean outgoing to userland */
  int in_fifo_minor; 
#endif
  int spike_channel;
  int ao_chan; /* specified by user interface */
  int magic;   /* should always equal AVN_SHM_MAGIC */
  int reserved[4]; /* just so i can look like i know what i am doing... */
};

#ifndef __cplusplus
  typedef struct AVNShared AVNShared;
#endif

#define AVN_SHM_NAME "AVN Stim Shared Memory"
#define AVN_SHM_MAGIC 0xdeadbeef

#define AVN_KERNEL_USER_FIFO_SZ (sizeof(double) * 100)
#define AVN_USER_KERNEL_FIFO_SZ (sizeof(struct AVNLiebnitz) * 100)

#ifdef __cplusplus
}
#endif

#endif
