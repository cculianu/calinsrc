/*
 * This file is part of the RT-Linux Multichannel Data Acquisition System
 *
 * Copyright (C) 2003 Calin Culianu
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

#ifndef _RTLAB_TYPES_H
#define _RTLAB_TYPES_H 1



/*
  RTLab Type definitions -- provides 32/64 bit neutrality using these type 
  aliases.

  In other words, these type aliases are conditional upon the 
  architecture we are on -- this is based on the kernel's own __sXX/__uXX 
  types defined in asm/types.h
*/

#ifdef __KERNEL__

/*  KERNEL */

/* Kernel headers seem to define stuff well so we will use their type
   aliases when in kernel mode                                               */
#  include <linux/types.h>

typedef __s32 int32;
typedef __u32 uint32;

typedef __s16 int16;
typedef __u16 uint16;

typedef __s8 int8;
typedef __u8 uint8;

typedef __s64 int64;
typedef __u64 uint64;


#else

/*  USER */

/* User headers may not have uint so we will define it here                  */
typedef unsigned int uint;

#  include <limits.h>

/* assumes gnu c!! */
#  if  LONG_MAX  ==   2147483647L 
#    define WORDSIZE 32
#  else 
#    define WORDSIZE 64
#  endif

#  if WORDSIZE == 64
typedef int int32;
typedef unsigned int uint32;
#  else
typedef long int int32;
typedef unsigned long int uint32;
#  endif

typedef short int16;
typedef unsigned short uint16;

typedef signed char int8;
typedef unsigned char uint8;

#  if WORDSIZE == 64
typedef signed long int    int64;
typedef unsigned long int uint64;
#  else
typedef signed long long int    int64;
typedef unsigned long long int uint64;
#  endif

#  undef WORDSIZE


#endif /* __KERNEL__ */


/* Used in SharedMemStruct                                                   */
typedef uint64 scan_index_t;
typedef uint   sampling_rate_t;

#endif
