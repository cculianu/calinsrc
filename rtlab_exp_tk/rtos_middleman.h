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

/* rtos_middleman.h
   Implementation of RTOS-specific routines.  This api attempts to 
   be a neutral bridge between different implementations of 
   RTOS api's.  Currently supports a subset of RTAI and RTLinux */
#ifndef RTOS_MIDDLEMAN_H
#define RTOS_MIDDLEMAN_H

#ifndef __KERNEL__
#error The rtos_middleman API is a bridging layer which is only be used in the kernel!
#endif

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>

#ifdef __cplusplus
extern "C" {
#endif

/* completely arbitrary */
#define RTOS_THREAD_STACK_MIN 8192

#if defined(__RTL__) && !defined(RTLINUX)
# define RTLINUX
#endif

#if defined(RTLINUX) 

# include <rtl.h>
# include <rtl_time.h>
# include <rtl_fifo.h>
# include <rtl_sched.h>
# include <mbuff.h>

extern int WARNING_THIS_MODULE_COMPILED_FOR_RTLINUX;

typedef hrtime_t rtos_time_t;

# define rtos_printf rtl_printf
static inline rtos_time_t rtos_get_time(void) { return gethrtime(); }

static inline unsigned long long ulldiv(unsigned long long ull, unsigned long uld, unsigned long *r)
{
        unsigned long long q, rf;
        unsigned long qh, rh, ql, qf;
 
        q = 0;
        rf = (unsigned long long)(0xFFFFFFFF - (qf = 0xFFFFFFFF / uld) * uld) + 1ULL;
 
        while (ull >= uld) {
                ((unsigned long *)&q)[1] += (qh = ((unsigned long *)&ull)[1] / uld);
                rh = ((unsigned long *)&ull)[1] - qh * uld;
                q += rh * (unsigned long long)qf + (ql = ((unsigned long *)&ull)[0] / uld);                ull = rh * rf + (((unsigned long *)&ull)[0] - ql * uld);
        }
 
        *r = ull;
        return q;
}

static inline void nano2timespec(hrtime_t time, struct timespec *t)
{
  t->tv_sec = ulldiv(time, 1000000000, (unsigned long *)&t->tv_nsec);
}

/* just like clock_gethrtime but instead it used timespecs */
static inline void clock_gettime(clockid_t clk, struct timespec *ts)
{
  hrtime_t now = clock_gethrtime(clk);

  nano2timespec(now, ts);  
}

#elif defined(RTAI)

# include <rtai_sched.h>
# include <rtai_pthread.h>

extern int WARNING_THIS_MODULE_COMPILED_FOR_RTAI;

typedef RTIME rtos_time_t;
typedef rtos_time_t hrtime_t;
# define HRTIME_INFINITY RT_TIME_END

# define rtos_printf rt_printk
# define rtl_printf rtos_printf
# define TIMER_ABSTIME 1

typedef int clockid_t;

/* HACK -- 
   only supports flags == TIMER_ABSTIME and clock_id == CLOCK_REALTIME 
   last param is ignored */
extern int clock_nanosleep(clockid_t clock_id, int flags,
                           const struct timespec *rqtp, struct timespec *rmtp);

#define NSECS_PER_SEC 1000000000
#define timespec_normalize(t) {\
        if ((t)->tv_nsec >= NSECS_PER_SEC) { \
                (t)->tv_nsec -= NSECS_PER_SEC; \
                (t)->tv_sec++; \
        } else if ((t)->tv_nsec < 0) { \
                (t)->tv_nsec += NSECS_PER_SEC; \
                (t)->tv_sec--; \
        } \
}

static inline void timespec_add_ns(struct timespec *t, long n)
{
        t->tv_nsec += n;  
        timespec_normalize(t); 
}

#define gethrtime rtos_get_time

#define mbuff_alloc rtos_shm_attach
#define mbuff_free(name,addr) rtos_shm_detach(addr)


/* hackish wrapper to pthread_create.. pthread_create gets #defined
   to pthread_create_rtai a few lines below... */
extern int pthread_create_rtai(pthread_t *thread, pthread_attr_t *attr,
                                void *(*start_routine) (void *), void *arg);
/* pthread_join is not implemented in RTAI! */
extern  int pthread_join(pthread_t th, void **thread_return);
extern  int pthread_cancel(pthread_t th);
extern  int pthread_attr_setfp_np(pthread_attr_t *attr, int use_fp); 
extern  int pthread_attr_setstackaddr(pthread_attr_t *, void *);
extern  int pthread_attr_setstacksize(pthread_attr_t *attr, size_t stacksize);


/* hackish replacements to implement missing functionality in rtai's posix */
#define RTL_PTHREAD_STACK_MIN RTOS_THREAD_STACK_MIN
#define rtf_create rtos_fifo_create
#define rtf_destroy rtos_fifo_destroy
#define rtf_put rtos_fifo_put
#define rtf_get rtos_fifo_get
#define RTF_NO RTOS_RTF_NO
#define pthread_create pthread_create_rtai

static inline rtos_time_t rtos_get_time(void) { return count2nano(rt_get_time()); }


#else
#  error Must define one of RTAI or RTLINUX to use this header file!
#endif


extern unsigned int RTOS_RTF_NO; /* max no. of fifos in the rtos, never
                                    pass a fifo >= this to  rtos_fifo_* funcs*/


extern int rtos_fifo_create(unsigned int fifo, int size);
extern int rtos_fifo_destroy(unsigned int fifo);
extern int rtos_fifo_get(unsigned int fifo, void *buf, int count);
extern int rtos_fifo_put(unsigned int fifo, void *buf, int count);

/* allocates/attaches to a shm buff named name */
extern void *rtos_shm_attach(const char *name, int size);
extern void rtos_shm_detach(void *addr);

static inline rtos_time_t to_rtos_time(unsigned long nanos) 
{
  rtos_time_t ret = nanos; /* is this a sufficient conversion? */
  return ret;
}



static void dont_call_me(void)
{
  int *x = 
#if defined(RTAI)
    &WARNING_THIS_MODULE_COMPILED_FOR_RTAI;
#elif defined(RTLINUX)
    &WARNING_THIS_MODULE_COMPILED_FOR_RTLINUX;
#endif

  if (!x) return;

  (*x)--;
  dont_call_me();
}

#ifdef __cplusplus
}
#endif


#endif
