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
#define EXPORT_SYMTAB 1
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <asm/atomic.h>
#include <asm/bitops.h>
#include <asm/semaphore.h>
#define RTLAB_INTERNAL
#include "rt_process.h"
#include "stimulator.h"

#define STIM_MAGIC (0x571801A7) /* Funny way to write STIMULAT */
#define TRAINS_2_N_CMDS(n) ((n) * 2 + 1)
struct rtlab_stimulator {
  int magic;
  struct list_head l;
  struct rtlab_comedi_context ctx;
  struct rtlab_stim_params params;
  rtlab_cmd_callback_t *callback;
  void *callback_arg;
  struct rtlab_cmd_handle *cmd_h; /* opaque handle to rtlab_cmd.c 
                                       functionality */
  struct rtlab_cmd *cmds;
  unsigned int max_train_sz;
  spinlock_t slock; /* spinlock for this stimulator is this needed? */
  atomic_t active; /* active flag  -- nonzero iff there is a stim going on 
                      zero if cancelled/stim ended */
  char was_in_realloc;
};

static struct rtlab_stimulator stim_list_head = { 
  magic:           STIM_MAGIC,
  l:               LIST_HEAD_INIT(stim_list_head.l),
  ctx:             EMPTY_CONTEXT,
  params:          EMPTY_STIM_PARAMS,
  callback:        0, 
  callback_arg:    0,
  cmd_h:           0,
  cmds:            0,
  max_train_sz:    0,
  slock:           SPIN_LOCK_UNLOCKED,
  active:          ATOMIC_INIT(0),
  was_in_realloc:  0
};

/* stim list might be busy because of non-realtime modifications */
DECLARE_MUTEX(stim_list_lock);

EXPORT_SYMBOL(rtlab_stimulate);
EXPORT_SYMBOL(rtlab_cancel_stim);
EXPORT_SYMBOL(rtlab_stim_alloc);
EXPORT_SYMBOL(rtlab_stim_free);
EXPORT_SYMBOL(rtlab_stim_set_context);
EXPORT_SYMBOL(rtlab_stim_set_callback);
EXPORT_SYMBOL(rtlab_default_stimulator);

/* Exported ... */
struct rtlab_stimulator * rtlab_default_stimulator;

static void stim_reaper(void *);
static int  stim_create_cmds(struct rtlab_stimulator *);
static void alloc_cmds(struct rtlab_stimulator *);


int init_stim_engine(void)
{
  struct rtlab_comedi_context ctx;

  ctx.dev = rtp_comedi_ao_dev_handle;
  ctx.subdev = comedi_find_subdevice_by_type(ctx.dev, COMEDI_SUBD_AO, 0);
  ctx.chan = 0;
  ctx.range = 0;
  ctx.aref = AREF_GROUND;
  
  if (! (DEFAULT_STIMULATOR = 
         rtlab_stim_alloc(&ctx, DEFAULT_STIMUALTOR_NUM_PER_TRAIN)) ) 
    return -ENOMEM; 

  return 0;
}

static void alloc_cmds(struct rtlab_stimulator *s)
{
  int err = 0;
  unsigned int n_cmds = TRAINS_2_N_CMDS(s->max_train_sz);

  struct rtlab_cmd *newcmds = 0;
  
  newcmds = (struct rtlab_cmd *)
    kmalloc(sizeof(struct rtlab_cmd) * n_cmds, GFP_KERNEL);

  if (!newcmds) { 
    err = ENOMEM; 
    s->cmds = 0;
    return;
  }

  memset(newcmds, 0, sizeof(struct rtlab_cmd) * n_cmds);
  
  s->cmds = newcmds;
}

struct rtlab_stimulator *rtlab_stim_alloc(const struct rtlab_comedi_context *c,
                                          unsigned int max_num_trains)
{
  struct rtlab_stimulator *ret = (struct rtlab_stimulator *)
    kmalloc(sizeof(struct rtlab_stimulator), GFP_KERNEL);
  struct rtlab_cmd_handle *h = rtlab_cmd_handle_alloc(TRAINS_2_N_CMDS(max_num_trains));
  
  if (!ret) return ret;
  if (!h) { kfree(ret); return 0; }

  /* give defaults from stim_list_head */
  memcpy(ret, &stim_list_head, sizeof(struct rtlab_stimulator));

  /* allocate cmd list */
  alloc_cmds(ret);
  if (!ret->cmds) { rtlab_cmd_handle_free(h); kfree(ret); return 0; }

  ret->cmd_h = h;

  /* set up this rtlab_stimulator with passed-in values */
  ret->max_train_sz = max_num_trains;
  memcpy(&ret->ctx, c, sizeof(struct rtlab_comedi_context));

  down(&stim_list_lock);
  list_add(&ret->l, &stim_list_head.l);
  up(&stim_list_lock);

  return ret;
}

void rtlab_stim_free(struct rtlab_stimulator *s)
{
  if (!s || s->magic != STIM_MAGIC) return;

  rtlab_cancel_stim(s);
  down(&stim_list_lock);
  list_del(&s->l);
  up(&stim_list_lock);

  rtlab_cmd_handle_free(s->cmd_h);
  kfree(s->cmds);

  /* invalidate the memory, then free */
  s->magic = 0;
  kfree(s);
}

#ifndef list_for_each_safe
 /* list_for_each_safe   -       
    iterate over a list safe against removal of list entry
    Stolen from newer linux kernels as older kernels lack this macro
  * @pos:        the &struct list_head to use as a loop counter.
  * @n:          another &struct list_head to use as temporary storage
  * @head:       the head for your list.
  */
#define list_for_each_safe(pos, n, head) \
        for (pos = (head)->next, n = pos->next; pos != (head); \
                pos = n, n = pos->next)
#endif
void cleanup_stim_engine(void) 
{ 
  struct list_head *pos, *n;
  struct rtlab_stimulator *stimulator;


  /* delete all the existing stim. params */
  list_for_each_safe(pos, n, &stim_list_head.l) {
    stimulator = list_entry(pos, struct rtlab_stimulator, l);
    rtlab_stim_free(stimulator); /* does implicit list_del() */
  }

}

/* 
   reregs stims at the end of a train that have trains left,
   or reaps zombie stims
   also responsible for doing callbacks..
*/
static void stim_reaper(void *arg)
{
  struct rtlab_stimulator *stim = (struct rtlab_stimulator *)arg;
  struct rtlab_stim_params *p;


  /* TODO SYNCHRONISATION HERE!!! This HAS A RACE CONDITION
     WITH NON-REALTIME */

  if (stim->magic != STIM_MAGIC) {
    rtos_printf(RT_PROCESS_MODULE_NAME": BUG! Illegal stim arg %x.\n", stim);
  } else if (atomic_read(&stim_list_lock.count) != 1) {
    /* stim list is busy, so postpone this callback by re-registering it! 
       note that this breaks realtime, but that is the tradeoff when 
       the user is installing new stim willy nilly :) */
    stim->cmds[0].type = RTLAB_CMD_CALLBACK;
    stim->cmds[0].when_ms = 1;
    stim->cmds[0].p1.callback = stim_reaper;
    stim->cmds[0].p2.callback_arg = (void *)stim;
    rtlab_cmd_register(stim->cmd_h, stim->cmds, 1);
    return;
  } else if (atomic_read(&stim->active)) {
#ifdef DEBUG
    // DEBUG
    rtos_printf("stimulator: in stim_reaper().. stim is %x\n", stim);
#endif
    p = &stim->params;

    if (p->num_trains > 0) p->num_trains--;

    /* end of a finite train  ... */
    if (p->num_trains == 0) {
      atomic_set(&stim->active, 0);
      if (stim->callback) stim->callback(stim->callback_arg);
      goto ret;
    }

    /* end of a continuous train */
    if (p->num_trains < 0 && stim->callback) {
#ifdef DEBUG
      //DEBUG
      rtos_printf("stimulator: stim_reaper: about to do continuous callback with %x %x..\n", stim->callback, stim->callback_arg);
#endif
      stim->callback(stim->callback_arg);    
    }

    p->when_ms = 0; /* make sure the next time we stim_create_cmds, 
                       it happens immediately */

    if (atomic_read(&stim->active) && stim_create_cmds(stim)) {
      /* command creation failed, so clear the stim */
      atomic_set(&stim->active, 0);
      goto ret;
    }
  } else {
    /* Must have been cancelled!  Note that if cancelled we do the
       last callback! */
    if (stim->callback) stim->callback(stim->callback_arg);
  }

 ret:
#ifdef DEBUG
  //DEBUG
  rtos_printf("stimulator: exiting stim_reaper()..\n");
#endif
}

static int stim_create_cmds(struct rtlab_stimulator *stim)
{
  const struct rtlab_stim_params *p = &stim->params;

  /* number of commands: 
     on phase, off phase * number per train + 1 (1 is the callback wrapper) */
  int num_cmds = TRAINS_2_N_CMDS(p->num_per_train);
  int time = p->when_ms;
  char is_attack = 1;
  int ret, i;
  struct rtlab_cmd *cmds = stim->cmds;    

  for (i = 0; num_cmds; i++) {
    struct rtlab_cmd *c = &cmds[i];

    c->when_ms = time;

    if (num_cmds == 1) { /* the callback */
      c->type = RTLAB_CMD_CALLBACK;
      /* do the fake callback wrapper -- either does cleanup or 
         sets up the next train */
      c->p1.callback = stim_reaper;
      c->p2.callback_arg = (void *)stim;
    } else if (is_attack) {
      c->type = RTLAB_CMD_AO;
      c->p1.ctx = &stim->ctx;
      c->p2.data = rtlab_volts_to_lsampl(c->p1.ctx, p->on_voltage); 
    } else { /* is not attack but sustain/decay */
      c->type = RTLAB_CMD_AO;
      c->p1.ctx = &stim->ctx;
      c->p2.data = rtlab_volts_to_lsampl(c->p1.ctx, p->off_voltage);
    }

    num_cmds--;
    is_attack = !is_attack; /* swap is_attack */

    /* update time for next iter */
    if (num_cmds == 1) {
      time += p->end_silence_ms;
    } else if (is_attack) {
      time += p->spacing_ms;
    } else {
      time += p->duration_ms;
    }

  }
  ret = rtlab_cmd_register(stim->cmd_h, cmds, num_cmds);
  
  return ret;
}

int rtlab_cancel_stim(struct rtlab_stimulator *stim)
{

  if (!stim || stim->magic != STIM_MAGIC || !atomic_read(&stim->active)) 
    return EINVAL;
  
  
  atomic_set(&stim->active, 0);
  return 0;
}

int rtlab_stimulate( struct rtlab_stimulator *stim,
                     const struct rtlab_stim_params *in)
{
  int err = 0;
  struct rtlab_comedi_context *ctx; 
  struct rtlab_stim_params *p; 

  if (!stim) stim = DEFAULT_STIMULATOR;

  ctx = &stim->ctx; 
  p = &stim->params;

  if (atomic_read(&stim->active)) {
#ifdef DEBUG
    // DEBUG
    rtos_printf("stimulator: Stimulator %x is busy\n", stim);
#endif
    return EBUSY;
  }
 
  /* -------- sanity checks ------ */

  /* are both on and off voltage ok? 
     can't verify off voltage without copying the context
     and seeing if they match and then trying various things..
     so we will assume that off voltage is ok for this range :/ */
  if ( (err = rtlab_find_and_set_best_range(ctx, in->on_voltage)) ) 
    goto end_error;
  
  
  if (in->when_ms < 0 || in->num_per_train < 0 
      || in->num_per_train > stim->max_train_sz) 
    { err = EINVAL; goto end_error; }

  memcpy(p, in, sizeof(struct rtlab_stim_params));
  
  if ( (err = stim_create_cmds(stim)) ) goto end_error;

  atomic_set(&stim->active, 1);

 end_error:
  return err;
}

int rtlab_stim_set_context(struct rtlab_stimulator *s, 
                           const struct rtlab_comedi_context *c)
{
  if (atomic_read(&s->active)) {
    return EAGAIN;
  }
  if (!s || s->magic != STIM_MAGIC || !c) return EINVAL;

  memcpy(&s->ctx, c, sizeof(struct rtlab_comedi_context));

  return 0;
}

int rtlab_stim_set_callback(struct rtlab_stimulator *s,
                            rtlab_cmd_callback_t *callback,
                            void *callback_arg)
{
  if (!s || !s->magic == STIM_MAGIC || !callback) return EINVAL;

  s->callback = callback;
  s->callback_arg = callback_arg;

  return 0;
}
