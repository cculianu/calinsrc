/**
 * userspace emulation -- just some wrappers to provide user-space malloc
 *  facilities in kernel space.  Wrappers to kmalloc and kfree, so that 
 *  user space algorithms, like bheap.c could be ported to kernel space
 */
#ifndef USERSPACE_COMPAT_H
#define USERSPACE_COMPAT_H

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/string.h>

static inline void *malloc(size_t size)
{
  return kmalloc(size, GFP_KERNEL);
}

static inline void *calloc(size_t nmemb, size_t size)
{
  void *memory = malloc(nmemb * size);
  
  if (memory)
    memset(memory, 0, nmemb * size);
  return memory;
}

static void free(void *m)
{
  kfree(m);
}

typedef asmlinkage int (*printf_t)(const char * fmt, ...);

static const printf_t printf = printk;

#endif
