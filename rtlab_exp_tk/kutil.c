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
#define EXPORT_SYMTAB 1
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <asm/div64.h>
#include "kutil.h"


EXPORT_SYMBOL_NOVERS(uint64_to_cstr);
EXPORT_SYMBOL_NOVERS(int64_to_cstr);
EXPORT_SYMBOL_NOVERS(i64str);
EXPORT_SYMBOL_NOVERS(ui64str);
EXPORT_SYMBOL_NOVERS(float2string);

/** Declare static local variables for circular static buffer functions.
    @param nbuffers the number of buffers
    @param strsize the size of each string in the buffers array
*/
#define DECLARE_CIRCBUFS(nbuffers, strsize) \
    static char buffers____[nbuffers][strsize]; \
    static const unsigned int numbufs____ = nbuffers; \
    static unsigned int bufctr____ = (nbuffers-1)
/** Grab the next string in the circular stringbuffers array. */
#define CIRCBUFS_NEXT \
                      buffers____[(bufctr____ = \
                       (++bufctr____) % numbufs____)]

int uint64_to_cstr(uint64 num, char *buf)
{
  static const uint64 ZEROULL = 0ULL;
  static const uint32 dividend = 10;
  int ct = 0, i;
  uint64 quotient = num;
  unsigned long remainder;
  char reversebuf[UINT64_BUFSZ];

  /* convert to base 10... results will be reversed */
  do {
    remainder = do_div(quotient, dividend);
    reversebuf[ct++] = remainder + '0';
  } while (quotient != ZEROULL);

  /* now reverse the reversed string... */
  for (i = 0; i < ct; i++) 
    buf[i] = reversebuf[(ct-i)-1];
  
  /* add null... */
  buf[ct] = 0;
  
  return ct;
}

int int64_to_cstr(int64 num, char *buf)
{
  static const int64 ZEROLL = 0LL;
  int ret = 0;
  uint64 u_num = (uint64)num;

  if (num < ZEROLL) {
    *buf = '-';
    buf++;
    u_num = (uint64)-num;
  }
  ret = uint64_to_cstr(u_num, buf);
  return ret;
}

const char * ui64str (uint64 num)
{
  DECLARE_CIRCBUFS(UI64_STR_N_BUFS, UINT64_BUFSZ);
  char * buf = CIRCBUFS_NEXT;

  if (uint64_to_cstr(num, buf) < 0)
    return 0;
  else
    return buf;
}

const char * i64str (int64 num)
{
  DECLARE_CIRCBUFS(I64_STR_N_BUFS, INT64_BUFSZ);
  char * buf = CIRCBUFS_NEXT;

  if (int64_to_cstr(num, buf) < 0)
    return 0;
  else
    return buf;
}

/** 
 *  float2string --
 *  
 *  Takes a float, f, and writes its decimal character string representation 
 *  to the memory pointed to by the first param.
 *
 *  Note that if the string ends up being exactly n characters long, 
 *  the trailing \0 is not written to the destination buffer 
 *  (as per strncpy).
 *
 *  Note that a float as a decimal string can never exceed 23 characters, 
 *  including trailing \0.
 *
 *  
 *  @param f the float to be expressed as a string
 *  @param dest the destination string buffer
 *  @param n is the size of the destination string buffer.  
 *  @param num_decs is the number of decimal places to the right of the decimal
 *  point required (trailing zeroes will be added).
 *
 *  @return the number of characters actually printed is returned, 
 *  not including the trailing \0.
 *
 */
int float2string(float f, char *out, int n, int num_decs)
{
        static const int sz = 23;
        char buf[sz]; /* temporary buffer - no string can be longer than 23
                         because ints are 32 bit, ergo 10 places for ordinal
                         and 10 places for mantissa = 20
                         plus the period and \0 = 22
                         plus one place for the optional minus '-' sign = 23 */
        unsigned int mantissa, multiple = 0;
        int ordinal, ret;
 
        if (n <= 0) return -EINVAL;
 
        if ((num_decs < 0) || (num_decs > 9)) return -EINVAL;
 
 
        ordinal = f;
 
        while(num_decs--) multiple = (multiple ? multiple * 10 : 10);
        mantissa = ( (f - (float)ordinal) * (float)multiple );
 
        ret = sprintf(buf, "%d.%u", ordinal, mantissa);
 
        buf[sz-1] = 0;
 
        strncpy(out, buf, n);
        return (ret < n ? ret : n);
}


/** 
 *  float2str --
 *  
 *  Identical to the above function except that it cycles through
 *  FLOAT2STR_N_BUFS static buffers and returns a pointer to one of them.
 *
 *  
 *  @param f the float to be expressed as a string
 *  @param num_decs is the number of decimal places to the right of the decimal
 *  point required (trailing zeroes will be added).
 *
 *  @return pointer to a static string buffer.  FLOAT2STR_N_BUFS is the number
 *  of buffers successive calls to this function cycle through.
 *
 */
const char * float2str(float f, int num_decs)
{  
#define FLOAT2STR_BUFSZ 25
    DECLARE_CIRCBUFS(FLOAT2STR_N_BUFS, FLOAT2STR_BUFSZ);
    char *buf = CIRCBUFS_NEXT;

    float2string(f, buf, FLOAT2STR_BUFSZ, num_decs);

    return buf;
#undef FLOAT2STR_BUFSZ
}
