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
  RTLab Type definitions -- some day we will be 32/64 bit neutral
  using these type aliases.

  In other words, these type aliases might some day be conditional upon the 
  architecture we are on -- but for now they just are basic and assume we are 
  on an IA32-like architechture. 
  (That is, a 32-bit arch with a 64 bit long long type.)
*/
#ifdef __KERNEL__
/* Kernel headers seem to define uint so we will make sure we have uint from
   kernel headers in kernel mode                                             */
#  include <linux/types.h>
#else 
/* User headers may not have uint so we will define it here                  */
typedef unsigned int uint;
#endif

typedef int int32;
typedef unsigned int uint32;

typedef short int16;
typedef unsigned short uint16;

typedef char int8;
typedef unsigned char uint8;

typedef unsigned long long int uint64;
typedef long long int int64;


/* Used in SharedMemStruct                                                   */
typedef uint64 scan_index_t;
typedef uint   sampling_rate_t;

#endif
