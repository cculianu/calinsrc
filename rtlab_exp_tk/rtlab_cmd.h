/*
 * This file is part of the RT-Linux Multichannel Data Acquisition System
 *
 * Copyright (C) 1999,2000 David Christini
 * Copyright (C) 2001,2002 Calin Culianu, David Christini
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
#ifndef _RTLAB_CMD_H
#define _RTLAB_CMD_H

#include <linux/list.h>
#include "rt_process.h"

enum rtlab_cmd_type {
  RTLAB_CMD_AO = 0,
  RTLAB_CMD_AI,
  RTLAB_CMD_CALLBACK
};

typedef void (rtlab_cmd_callback_t)(void *arg);


struct rtlab_cmd 
{
  enum rtlab_cmd_type type; /* what type of command */
  int when_ms; /* when to execute this command (in the future.. 0 == NOW */
# define RTLAB_CMD_NOW 0 /* used for when_ms parameter to indicate immediate */
  union { /* -- what fn callback or what context to use */
    rtlab_cmd_callback_t *callback;
    const struct rtlab_comedi_context *ctx;
  } p1; /* param 1 */ 
  union { /* -- where to put results or what arg to use */
    void *callback_arg; /* use this fro RTLAB_CMD_CALLBACK */
    void *generic; /* not currently used */
    lsampl_t *data_out; /* use this for RTLAB_CMD_AI */
    lsampl_t data; /* use this for RTLAB_CMD_AO */
  } p2; /* param 2  */
};

/* 
 *  Do not call these -- called by rtlab.o only! 
 *  NOT EXPORTED!!
 */
#ifdef RTLAB_INTERNAL
extern int  init_cmd_engine(void); /* called once from non-realtime context */
extern void rtlab_cmd_process(void); /* called each time inside realtime loop*/
extern void cleanup_cmd_engine(void); /* called once from non-realtime context */
#endif


/* Opaque type used by some of the method functions below */
struct rtlab_cmd_handle;

/*----------------------------------------------------------------------------
  EXPORTED FUNCTIONS/VARIABLES 
-----------------------------------------------------------------------------*/
/* 
   rtlab_cmd_register()

   Registers a command array of size n_cmds.

   Returns E2BIG if there is no more room in the command
   queue buffer, otherwise returns 0 on success 

   Note that the const struct rtlab_cmd array must be persistent
   until the command actually runs, which means that you should make sure
   to not write to the rtlab_cmd's in the array until the last command
   has been executed.

   Currently supports call from Realtime context only!
*/
extern int  rtlab_cmd_register(struct rtlab_cmd_handle *, 
                               const struct rtlab_cmd *array, 
                               uint n_cmds);
extern struct rtlab_cmd_handle *rtlab_cmd_handle_alloc(unsigned int max_cmds);
extern void   rtlab_cmd_handle_free(struct rtlab_cmd_handle *);
#endif
