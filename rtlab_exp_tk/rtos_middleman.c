/*
 * This file is part of the RT-Linux Multichannel Data Acquisition System
 *
 * Copyright (C) 1999,2000 David Christini
 * Copyright (C) 2001 Calin Culianu
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

/* rtos_middleman.c
   Implementation of RTOS-specific routines.  This api attempts to 
   be a neutral bridge between different implementations of 
   RTOS api's.  Currently supports a subset of RTAI and RTLinux */
#define EXPORT_SYMTAB 1 /* <--- for annoying kernels that need this */
#include "rtos_middleman.h"

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <asm/bitops.h>
#include <linux/list.h>
#include <asm/atomic.h>
#include <asm/semaphore.h>
#include <linux/spinlock.h>

/*-----------------------------------------------------------------------------
  Some globally exported symbols
-----------------------------------------------------------------------------*/
EXPORT_SYMBOL_NOVERS(rtos_fifo_create);
EXPORT_SYMBOL_NOVERS(rtos_fifo_destroy);
EXPORT_SYMBOL_NOVERS(rtos_fifo_put);
EXPORT_SYMBOL_NOVERS(rtos_fifo_get);
EXPORT_SYMBOL_NOVERS(rtos_shm_attach);
EXPORT_SYMBOL_NOVERS(rtos_shm_detach);


/*-----------------------------------------------------------------------------
  RTOS-specific declarations
-----------------------------------------------------------------------------*/
#if defined(RTLINUX)
# include <rtl_sched.h>
# include <rtl_fifo.h>
# include <mbuff.h>

/* dummy to ensure insmod fails on failed symbols on mismatch between
   rtos_middleman.o and any modules compiled afterwards that depend on it */
EXPORT_SYMBOL_NOVERS(WARNING_THIS_MODULE_COMPILED_FOR_RTLINUX);
int WARNING_THIS_MODULE_COMPILED_FOR_RTLINUX = 0;

#define RTF_NO_ RTF_NO

#elif defined(RTAI)

EXPORT_SYMBOL_NOVERS(pthread_create_rtai);
EXPORT_SYMBOL_NOVERS(pthread_join);
EXPORT_SYMBOL_NOVERS(pthread_cancel);
EXPORT_SYMBOL_NOVERS(pthread_attr_setfp_np);
EXPORT_SYMBOL_NOVERS(pthread_attr_setstackaddr);
EXPORT_SYMBOL_NOVERS(pthread_attr_setstacksize);
EXPORT_SYMBOL_NOVERS(clock_nanosleep);
EXPORT_SYMBOL_NOVERS(RTOS_RTF_NO);

/* dummy to ensure insmod fails on failed symbols on mismatch between
   rtos_middleman.o and any modules compiled afterwards that depend on it */
EXPORT_SYMBOL_NOVERS(WARNING_THIS_MODULE_COMPILED_FOR_RTAI);
int WARNING_THIS_MODULE_COMPILED_FOR_RTAI = 0;

/* clear some aliases */
# undef rtf_create
# undef rtf_destroy
# undef rtf_get
# undef rtf_put
# undef pthread_create
# undef RTF_NO

# include <rtai_fifos.h>
# include <rtai_shm.h>
# include <rtai_sched.h>

/* grrr.. this isn't defined in the headers so I have to hard code it here */
#define RTF_NO_ 64

#endif
/*-----------------------------------------------------------------------------
  Common declarations
-----------------------------------------------------------------------------*/

unsigned int RTOS_RTF_NO = RTF_NO_;


/*-----------------------------------------------------------------------------
  Common definitions
-----------------------------------------------------------------------------*/

#define FIFO_BUSY_FLAGS_SZ (RTF_NO_ / sizeof(char) + 1)
static volatile char fifo_busy_flags[FIFO_BUSY_FLAGS_SZ] = {0};

/* the following four functions use implementations that make exactly the 
   same calls in both rtlinux and rtai */
int rtos_fifo_create(unsigned int fifo, int size)
{

  if (fifo > RTOS_RTF_NO) return -EINVAL;

  /* below is atomic */ 
  if (test_and_set_bit(fifo, fifo_busy_flags)) return -EBUSY;
  return rtf_create(fifo, size); 
}

int rtos_fifo_destroy(unsigned int fifo)
{
  /* atomic */
  int is_allocated_already;

  if (fifo > RTOS_RTF_NO) return -EINVAL;

  is_allocated_already = test_and_clear_bit(fifo, fifo_busy_flags);

  if (!is_allocated_already) 
    rtos_printf("Warning: rtos_fifo_destroy called on already-destroyed fifo "
                "with minor number: %u\n", fifo);
  return rtf_destroy(fifo);
}

int rtos_fifo_get(unsigned int fifo, void *buf, int count)
{
  return rtf_get(fifo, buf, count);
}

int rtos_fifo_put(unsigned int fifo, void *buf, int count)
{
  return rtf_put(fifo, buf, count);
}

/*-----------------------------------------------------------------------------
  RTOS-specific definitions
-----------------------------------------------------------------------------*/

#if defined(RTLINUX)

#include <linux/string.h>

struct mbuff_map {
  struct list_head lh;
  char name[MBUFF_NAME_LEN+1];
  void *addr;
};

struct mbuff_map mbuff_maps = { 
  lh : { (struct list_head *)&mbuff_maps, 
         (struct list_head *)&mbuff_maps }, 
  name : { 0 }, 
  addr : 0 
};

DECLARE_MUTEX(mbuff_map_sem);

/* allocates/attaches to a shm buff named name */
void *rtos_shm_attach(const char *name, int size)
{
  struct mbuff_map *new_map;

  char * mbuff = (char *)mbuff_alloc(name, size);
  
  if (!mbuff) return 0;

  new_map = kmalloc(sizeof(struct mbuff_map), GFP_KERNEL);

  if (!new_map) { mbuff_free(name, mbuff); return 0; }

  strncpy(new_map->name, name, MBUFF_NAME_LEN) [ MBUFF_NAME_LEN ] = 0;
  new_map->addr = mbuff;

  down(&mbuff_map_sem);
  list_add((struct list_head *)new_map, (struct list_head *)&mbuff_maps);
  up(&mbuff_map_sem);

  return mbuff;
}

void rtos_shm_detach(void *addr) 
{
  struct list_head *cur;
  const struct list_head *lhead = ((struct list_head *)&mbuff_maps);
  struct mbuff_map *cur_map;

  down(&mbuff_map_sem);
  list_for_each(cur, lhead) {
    cur_map = (struct mbuff_map *)cur;
    if (cur_map->addr == addr) break; /* match! */
  }
  
  if (cur != lhead) { /* if we actually found something */
    mbuff_free(cur_map->name, addr);
    list_del(cur);
    kfree(cur);
  }
  up(&mbuff_map_sem);
}


#elif defined(RTAI)

struct shm_addr_nam_map
{
  struct list_head lh;
  unsigned long name;
  void *address;
};

struct shm_addr_nam_map shm_map = { 
  lh     : { (struct list_head *)&shm_map, 
             (struct list_head *)&shm_map }, 
  name   : 0, 
  address: 0 
};
DECLARE_MUTEX(shm_map_mutex);

/* allocates/attaches to a shm buff named name */
void *rtos_shm_attach(const char *name, int size)
{
  unsigned long name_i = nam2num(name);  
  /* need to prepend the buf's name to the region.. */
  unsigned long *buf =  (unsigned long *) rtai_kmalloc(name_i, size);
  
  if (buf) {
    struct shm_addr_nam_map *new_map = kmalloc(sizeof(struct shm_addr_nam_map),
                                               GFP_KERNEL);    
    if (!new_map) {
      rtai_kfree(name_i);
      return 0;
    }
    new_map->name = name_i;
    new_map->address = buf;

    down(&shm_map_mutex);
    list_add((struct list_head *)new_map, (struct list_head *)&shm_map);
    up(&shm_map_mutex);
  }

  return buf; /* return the valid region */
}

void rtos_shm_detach(void *addr)
{
  struct list_head *cur;
  const struct list_head *lhead = ((struct list_head *)&shm_map);
  struct shm_addr_nam_map *map = 0;
  unsigned long name = 0;

  down(&shm_map_mutex);

  list_for_each(cur, lhead) {
    map = (struct shm_addr_nam_map *)cur;
    if ( map->address == addr ) { name = map->name;  break;  }
  }

  /* if we actually found something */
  if (cur != lhead)  {
    list_del(cur); /* cur == map, but with a different type */
    kfree(cur);
    rtai_kfree(name);
  }

  up(&shm_map_mutex);
}

int clock_nanosleep(clockid_t clock_id, int flags,
                    const struct timespec *rqtp, struct timespec *rmtp)
{
  RTIME expire;

  if (clock_id != CLOCK_REALTIME || flags != TIMER_ABSTIME
      || rqtp->tv_nsec >= 1000000000L || rqtp->tv_nsec < 0 
      || rqtp->tv_sec < 0)
    return -EINVAL; /* not supported or invalid params */
  
  expire = timespec2count(rqtp);

  if (expire <= rt_get_time()) {
    rtos_printf("BUG in rtos_middleman::clock_nanosleep()! Requested absolute "
                "time is in the past!\n");
  } 

  rt_sleep_until(expire);

  if ((expire -= rt_get_time()) > 0) {
    if (rmtp) count2timespec(expire, rmtp);    
    return -EINTR;
  }
  return 0;
}

#define RT_POLLING_SLEEPTIME (nano2count(5000000)) /* 5ms realtime timeout */
#define POLLING_SLEEPTIME (HZ/10) /* 10 ms linux timeout */

struct rt_task_data {
  void *(*real_entry)(void *);
  void *real_arg;
  void *ret;
  volatile RT_TASK *task_struct;
  volatile pthread_t pthread_id;
  volatile atomic_t pthread_id_is_set;
  volatile atomic_t delete_me;
  volatile atomic_t task_struct_is_set;
  volatile atomic_t deleted;
};

#define NON_RT_POLL_COND(x) \
    do { \
        int POLL_I = 0; \
        printk("About to poll..\n"); \
        while (x) { \
          if (!(POLL_I++ % 100))printk("Looped in poll %d times.\n",POLL_I-1);\
          current->state = TASK_INTERRUPTIBLE; \
          schedule_timeout(POLLING_SLEEPTIME); \
        } \
    } while(0)

/* spins around, sleeping for a few ms each time, waiting to acquire
   the lock for SEM *x  -- note possible priority inversion here, since
   NON RT acquires a lock that RT uses as well! */
#define NON_RT_SEM_ACQUIRE(x) NON_RT_POLL_COND(rt_sem_wait_if(x) <= 0)

#define MAX_THREADS 10
static struct rt_task_data threads[MAX_THREADS];
static unsigned int threads_mask = 0;

static SEM threads_sem;
static atomic_t threads_stuff_init = ATOMIC_INIT(0);
DECLARE_MUTEX(threads_init_mut);


static void custom_rt_sh(void)
{
  pthread_t me;
  int i;

  me = pthread_self();
  rtos_printf("In signal handler for %ld...\n", me);
  rt_task_signal_handler(rt_whoami(), custom_rt_sh);

  rt_sem_wait(&threads_sem);

  for (i = 0; i < MAX_THREADS; i++) {
    rtos_printf("Looping no luck for thread %ld...\n", me);
    if (threads[i].pthread_id == me && atomic_read(&threads[i].delete_me)
        && test_bit(i, &threads_mask) ) {      
      rtos_printf("Luck! Reached in deletion code for thread %ld...\n", me);
      atomic_set(&threads[i].deleted, 1);
      rt_sem_signal(&threads_sem); /* need to clear the sem before 
                                      we can delete! Argh! Race condition? */
      pthread_exit(0);
      /* not reached */
      rtos_printf("Reached not reachable area...\n");
      return;
    }
  }

  rt_sem_signal(&threads_sem);
}

static void *dummy_pthread_entry_func_that_installs_signal_handler(void *in)
{
  struct rt_task_data *arg = (struct rt_task_data *) in;

  rt_sem_wait(&threads_sem);

  arg->task_struct = rt_whoami();  
  atomic_set(&arg->task_struct_is_set, 1);

  arg->pthread_id = pthread_self();
  atomic_set(&arg->pthread_id_is_set, 1);

  /* install our custom task-killing sighandler */
  rt_sem_signal(&threads_sem);

  //rt_task_signal_handler(rt_whoami(), custom_rt_sh);


  arg->ret = arg->real_entry(arg->real_arg);

  
  /* cleanup code here... */

  /* commented out because this point not always reached .. 
     we also release data in pthread_join()

     release_thread_data_rt(arg);
  */

  /* end cleanup code */
  pthread_exit(arg->ret);
  return arg->ret;
}

/* 
   finds a free thread_data entry and return it, properly initialized 
   make sure this is single-threaded with respect to the threads array!
*/
static struct rt_task_data * find_free_thread_data(void)
{
  int i, bit;
  struct rt_task_data *thread;

  for (i = 0; i < MAX_THREADS; i++) { 
    bit = test_and_set_bit(i, &threads_mask); 
    if (!bit) break;
  } 
 
  if (i >= MAX_THREADS) {
    return 0;
  }

  thread = threads + i;
  atomic_set(&thread->task_struct_is_set, 0);
  thread->task_struct = 0; /* so no realtime thread looks at this */
  
  atomic_set(&thread->pthread_id_is_set, 0);
  thread->pthread_id = 0;

  atomic_set(&thread->delete_me, 0);

  atomic_set(&thread->deleted, 0);

  return thread;
}

static inline void release_thread_data_nolock(struct rt_task_data *d)
{
  int index = d - threads;

  clear_bit(index, &threads_mask);
}

/* returns the passed-in rt_task_data back to the free list */ 
static void release_thread_data(struct rt_task_data *d)
{
  NON_RT_SEM_ACQUIRE(&threads_sem);
  
  release_thread_data_nolock(d);

  rt_sem_signal(&threads_sem);
}

#ifdef pthread_create
#undef pthread_create
#endif
int pthread_create_rtai(pthread_t *thread_out, pthread_attr_t *attr,
                        void *(*start_routine) (void *), void *arg)
{
  int ret;
  struct rt_task_data *thread;

  down(&threads_init_mut);
  /* this is called once per existance of this module */
  if (! atomic_read(&threads_stuff_init)) {
    rt_sem_init(&threads_sem, 1);
    stop_rt_timer();
    rt_set_periodic_mode();
    /* should this be tweakable? */
    start_rt_timer(nano2count(25000));

    atomic_set(&threads_stuff_init, 1);
  }  
  up(&threads_init_mut);
  
  
  /* spin here until we get the semaphore */
  NON_RT_SEM_ACQUIRE(&threads_sem);
  
  thread = find_free_thread_data();

  if (!thread)  return EAGAIN;
  
  thread->real_entry = start_routine;
  thread->real_arg = arg;

  ret = pthread_create(thread_out, attr, 
                       dummy_pthread_entry_func_that_installs_signal_handler, 
                       thread);


  if (ret)  /* failure ! */
    release_thread_data_nolock(thread);

  rt_sem_signal(&threads_sem);

  /* spin here until the realtime thread was called and it set up the
     pthread_id -- this guarantees that a pthread_join() called in the same
     thread after a pthread_create() will be able to find the 
     right thread... */
  if (!ret) {
    NON_RT_POLL_COND(!atomic_read(&thread->pthread_id_is_set));
    printk("Created thread %ld\n", thread->pthread_id);
  }
  
  return ret;
}


int pthread_join(pthread_t th, void **tr) 
{
  RT_TASK *task = 0;
  struct rt_task_data *thread = 0;
  int i;
  volatile int magic;


  printk("Joining on thread %ld\n", th);

  NON_RT_SEM_ACQUIRE(&threads_sem);

  /* search for the thread with id th in the threads[] array and set the 
     delete_me flag if found */
  for (i = 0; i < MAX_THREADS; i++) {
    if (test_bit(i, &threads_mask) && 
        atomic_read(&threads[i].pthread_id_is_set) &&
        atomic_read(&threads[i].task_struct_is_set) &&
        threads[i].pthread_id == th) {

      task = (RT_TASK *)threads[i].task_struct;
      thread = threads + i;
      break;
    }
  }

  rt_sem_signal(&threads_sem);

  if (!task)  return ESRCH; 

  printk("Polling task->magic...\n");
  printk(".......................................\n");

  /* poll until the task is freed.. */
  NON_RT_POLL_COND((magic = task->magic) == RT_TASK_MAGIC); 

  if (tr) *tr = thread->ret; /* this is not always reliable... */

  release_thread_data(thread);

  return 0;
}

int pthread_cancel(pthread_t th) 
{ 
  int i;

  NON_RT_SEM_ACQUIRE(&threads_sem);
  
  /* search for the thread with id th in the threads[] array and set the 
     delete_me flag if found */
  for (i = 0; i < MAX_THREADS; i++) {
    if (test_bit(i, &threads_mask) &&         
        atomic_read(&threads[i].pthread_id_is_set) &&
        threads[i].pthread_id == th) {
      atomic_set(&threads[i].delete_me, 1);
      break;
    }
  }

  rt_sem_signal(&threads_sem);
  
  if (i >= MAX_THREADS) return ESRCH;
  return 0;
}

int pthread_attr_setfp_np(pthread_attr_t *a, int b) { (void)a; (void)b; return 0; }
int pthread_attr_setstackaddr(pthread_attr_t *a, void *b) { (void)a; (void)b; return 0;}
int pthread_attr_setstacksize(pthread_attr_t *a, size_t b) { (void)a; (void)b; return 0;}

#endif /* defined RTAI */
