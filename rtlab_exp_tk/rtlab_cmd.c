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
#define EXPORT_SYMTAB 1 /* <--- for annoying kernels that need this */ 
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/spinlock.h>
#include <asm/bitops.h>
#include <asm/atomic.h>
#include <asm/semaphore.h>
#include <linux/string.h>
#define RTLAB_INTERNAL
#include "rt_process.h"
#include "rtlab_cmd.h"
#include "bheap.h" /* binary heap implements priority-based command list */

EXPORT_SYMBOL(rtlab_cmd_register);
EXPORT_SYMBOL(rtlab_cmd_handle_alloc);
EXPORT_SYMBOL(rtlab_cmd_handle_free);

#define HANDLE_MAGIC (0x4A9D1E)


typedef struct { 
  rtlab_cmd_callback_t *cb; 
  void *arg; 
} callback_cmd_q;

struct rtlab_cmd_handle {
  int magic;
  const struct rtlab_cmd **cmds;
  char *cmd_mask;
  callback_cmd_q *cmd_q;
  unsigned int q_sz;
  unsigned int mask_size;
  unsigned int cmds_size;
  unsigned int n_cmds; /* number actually allocated */
  bheap_t *bheap;
  struct list_head l;
  spinlock_t slock;
};

static struct rtlab_cmd_handle handle_list = {
  magic      : HANDLE_MAGIC,
  cmds       : 0,
  cmd_mask   : 0,
  cmd_q      : 0,
  q_sz       : 0,
  mask_size  : 0,
  cmds_size  : 0,
  n_cmds     : 0,
  bheap      : 0,
  l          : LIST_HEAD_INIT(handle_list.l),
  slock      : SPIN_LOCK_UNLOCKED
};

DECLARE_MUTEX(handle_list_lock);

/* returns the position in the cmds array of an unused struct rtlab_cmd * 
   or -1 on error*/
static int alloc_cmd(struct rtlab_cmd_handle *);
/* frees command in command array at position pos */
static void free_cmd(struct rtlab_cmd_handle *, unsigned int pos);


struct rtlab_cmd_handle *rtlab_cmd_handle_alloc(unsigned int max_cmds)
{
  struct rtlab_cmd_handle *ret = 0;
  const struct rtlab_cmd **cmds = 0;
  char *cmd_mask = 0;
  callback_cmd_q *cmd_q = 0;  
  bheap_t *heap = 0;
  unsigned int msk_sz;

  if (!max_cmds) goto err;

  if (!(ret=(struct rtlab_cmd_handle *)kmalloc(sizeof(struct rtlab_cmd_handle),
                                               GFP_KERNEL)) ||
      !(cmds = (const struct rtlab_cmd **)kmalloc(sizeof(struct rtlab_cmd *) 
                                            * max_cmds, GFP_KERNEL)) ||
      !(cmd_mask = (char *)kmalloc(sizeof(char) * (msk_sz = 
						   (max_cmds / 8 + 
						    (max_cmds % 8 ? 1 : 0))),
				   GFP_KERNEL)) ||
      !(cmd_q = (callback_cmd_q *)kmalloc(sizeof(callback_cmd_q) * max_cmds,
                                          GFP_KERNEL))||
      !(heap = bh_alloc(max_cmds))) 
    goto err;
  
  /* take default values... */
  memcpy(ret, &handle_list, sizeof(struct rtlab_cmd_handle));
  memset(cmd_mask, 0, msk_sz);
  
  ret->cmds = cmds;
  ret->cmd_mask = cmd_mask;
  ret->cmd_q = cmd_q;
  ret->bheap = heap;
  ret->mask_size = msk_sz;
  ret->cmds_size = max_cmds;

  down(&handle_list_lock);
  list_add(&ret->l, &handle_list.l);
  up(&handle_list_lock);

  return ret;
 err:
  if (ret) kfree(ret);
  if (cmds) kfree(cmds);
  if (cmd_mask) kfree(cmd_mask);
  if (cmd_q) kfree(cmd_q);
  if (heap) bh_free(heap);
  return 0;
}

void  rtlab_cmd_handle_free(struct rtlab_cmd_handle *h)
{
  if (!h || h->magic != HANDLE_MAGIC) return;

  down(&handle_list_lock);
  list_del(&h->l);
  up(&handle_list_lock);
  bh_free(h->bheap);
  kfree(h->cmds);
  kfree(h->cmd_mask);
  kfree(h->cmd_q);
  h->magic = 0;
  kfree(h);
}

int rtlab_cmd_register(struct rtlab_cmd_handle *h,
                       const struct rtlab_cmd *list, uint count)
{
  const struct rtlab_cmd *cmd = 0;
  unsigned int i;

  if (!h || h->magic != HANDLE_MAGIC) return EINVAL;

  if (count > (h->cmds_size - h->n_cmds) )  return E2BIG;
 
  for (i = 0; i < count; i++) {
    int pos;
    scan_index_t when;
    int when_ms;

    pos = alloc_cmd(h); /* 'allocate' a command from the semistatic array */

    if (pos < 0) {
#ifdef DEBUG
      rtos_printf("rtlab_cmd: BUG in rtlab_cmd_register!! Cannot allocate a "
                  "command! Cmds_size: %u N_cmds %u\n", h->cmds_size, 
                  h->n_cmds);
#endif
      return ENOSPC;
    }

    /* save the pointer */
    cmd = h->cmds[pos] = &list[i];

    switch(cmd->type) {
    case RTLAB_CMD_AO:
    case RTLAB_CMD_AI:
    case RTLAB_CMD_CALLBACK:
      break;
    default:
      rtos_printf("rtlab_cmd: Unknown/Unimplemented command type %x\n", 
                  cmd->type);
      return EINVAL;
      break;
    }

    if ( (when_ms = cmd->when_ms) < 0) when_ms = 0;

    when = rtp_shm->scan_index + (scan_index_t)(when_ms * MILLION 
                                                / rtp_shm->nanos_per_scan);
    bh_insert(h->bheap, pos, when);

#ifdef DEBUG
    rtos_printf("rtlab_cmd: Command %x will execute at %u (now it's %u)\n", 
                cmd, (unsigned int)when, (unsigned int)rtp_shm->scan_index);
#endif
  }
 
  return 0;
}

#define next_from_bheap(bheap, cmds, cmd, min, k) \
    (cmd = cmds[(min = bh_min(bheap))], \
    k = bheap->a[bheap->p[min]].key)

void rtlab_cmd_process(void) 
{
  struct rtlab_cmd_handle *h;
  struct list_head *pos;
  const struct rtlab_cmd *cmd;
  unsigned int i;
  int min;
  scan_index_t when;

  if (atomic_read(&handle_list_lock.count) != 1) {
    return; /* handle list is busy so skip it this time around.. this breaks
	       realtime but that's the best we can do.. */
  }

  list_for_each(pos, &handle_list.l) {
    h = list_entry(pos, struct rtlab_cmd_handle, l);

    if ( !h->n_cmds )  continue;
      
    for (next_from_bheap(h->bheap, h->cmds, cmd, min, when); 
         h->bheap->n && /* just incase bh_min() flows past end? */
         when <= rtp_shm->scan_index; 
         next_from_bheap(h->bheap, h->cmds, cmd, min, when)) {

      switch (cmd->type) {
      case RTLAB_CMD_AO:
#ifdef DEBUG
        // DEBUG
        rtos_printf("rtlab_cmd: About to do AO command [Dev %x Subdev %u Chan %u Range %u Aref %u Data %hu]\n",
                    cmd->p1.ctx->dev, cmd->p1.ctx->subdev, 
                    cmd->p1.ctx->chan,
                    cmd->p1.ctx->range, cmd->p1.ctx->aref, cmd->p2.data);
#endif
        comedi_data_write(cmd->p1.ctx->dev, cmd->p1.ctx->subdev, 
                          cmd->p1.ctx->chan,
                          cmd->p1.ctx->range, cmd->p1.ctx->aref, cmd->p2.data);
      break;
      case RTLAB_CMD_AI:
        comedi_data_read(cmd->p1.ctx->dev, cmd->p1.ctx->subdev, 
                         cmd->p1.ctx->chan, cmd->p1.ctx->range, 
                         cmd->p1.ctx->aref, cmd->p2.data_out);
        break;
      case RTLAB_CMD_CALLBACK:
        h->cmd_q[h->q_sz].cb = cmd->p1.callback;
        h->cmd_q[h->q_sz++].arg = cmd->p2.callback_arg;
        break;
      default:
#ifdef DEBUG
        //DEBUG
        rtos_printf("rtlab_cmd: BUG in rtlab_cmd::rtlab_cmd_process! "
                    "Unknown command type encountered (%x)\n", cmd->type);
#endif
        break;
      }
      free_cmd(h, min);
      bh_delete(h->bheap, min);
    }

#ifdef DEBUG
        // DEBUG temporary
        rtos_printf("rtlab_cmd: possible node off the bheap: min is %d, when is %u\n", min, (unsigned int)when);
#endif

    for (i = 0; i < h->q_sz; i++) {
#ifdef DEBUG
      // DEBUG
      rtos_printf("rtlab_cmd: About to do CALLBACK command "
                  "[function %x arg %x]\n",h->cmd_q[i].cb, 
                  h->cmd_q[i].arg);
#endif
      h->cmd_q[i].cb(h->cmd_q[i].arg);
    }
    h->q_sz = 0;
  }
}

int init_cmd_engine(void)
{
  return 0;
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
void cleanup_cmd_engine(void)
{
  struct list_head *pos, *n;
  struct rtlab_cmd_handle *h;

  list_for_each_safe(pos, n, &handle_list.l) {
    h = list_entry(pos, struct rtlab_cmd_handle, l);
    rtlab_cmd_handle_free(h); /* does implicit list_del() */
  }
}

static int alloc_cmd(struct rtlab_cmd_handle *h)
{
  int i = -1; 

  if (h->n_cmds < h->cmds_size) {

    i = find_first_zero_bit(h->cmd_mask, h->cmds_size);
    set_bit(i, h->cmd_mask);
    h->n_cmds++;
    
  }

  return i;
}

static void free_cmd(struct rtlab_cmd_handle *h, unsigned int pos)
{
  if (pos > h->cmds_size) return; /* invalid c */

  if (test_bit(pos, h->cmd_mask)) {
    h->n_cmds--;
    clear_bit(pos, h->cmd_mask);
  }
}
