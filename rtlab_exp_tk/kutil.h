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
#ifndef __KERNEL__
#error kutil.h is to be used only inside the kernel!
#endif
#ifndef _KUTIL_H
#define _KUTIL_H

#include "rtlab_types.h"

/** The maximum length the uint64_to_cstr function will ever return */
#define UINT64_MAX_STRLEN 20
/** The maximum length the int64_to_cstr function will ever return */
#define INT64_MAX_STRLEN  UINT64_MAX_STRLEN 
/** The recommented size of buffer 'buf' for function uint64_to_cstr */
#define UINT64_BUFSZ  (UINT64_MAX_STRLEN+1)
/** The recommented size of buffer 'buf' for function int64_to_cstr */
#define INT64_BUFSZ  (INT64_MAX_STRLEN+1)
/**
 * Stringify a 64-bit unsigned integer into a string buffer as 
 * a base-10 human-readable string. The buffer should have enough space
 * to hold the string, including terminating NULL.   To ensure this,
 * the preprocessor define UINT64_BUFSZ is provided.  This size is 
 * the maximum size buffer you will ever need to represent a 64-bit
 * unsigned integer in base 10.
 *
 * @param num the unsigned 64-bit number.
 * @param buf the buffer to write the string to, should be at least 
 *        UINT64_BUFSZ in size.
 * @return the length of the generated numerical string, or negative on error.
 */
extern int uint64_to_cstr(uint64 num, char *buf);
/**
 * Convert a signed 64 bit integer to a human-readable C-string.  Identical
 * To the above function and provided solely as a convenience.
 *
 * @param num the signed 64-bit number
 * @param buf the buffer to write the string to
 * @return the length of the generated numerical string, or negative on error.
 */
extern int int64_to_cstr (int64 num, char *buf);


/** The number of buffers the ui64str() function below cycles through. */
#define UI64_STR_N_BUFS 8
/** The number of buffers the i64str()  function below cycles through. */
#define I64_STR_N_BUFS UI64_STR_N_BUFS

/**
 * Stringify a 64-bit unsigned integer into a static string buffer as 
 * a base-10 human-readable string. The buffer is _not_ thread safe
 * but that may be OK in some contexts.  Note that this function cycles
 * through I64_STR_N_BUFS buffers, so the returned pointers may be
 * valid for that many calls to this function.
 *
 * @param num the unsigned 64-bit number
 * @return a pointer to the converted C-string. This is a pointer to a static
 *         buffer and is thus unreliable in a threaded context, 
 *         however there are UI64_STR_N_BUFS static buffers that this function
 *         cycles through, so if you are careful you can keep the pointer
 *         around for at least UI64_STR_N_BUFS calls to this function.
 */
extern const char * ui64str (uint64 num);
/**
 * Stringify a 64-bit signed integer into a static string buffer as 
 * a base-10 human-readable string. The buffer is _not_ thread safe
 * but that may be OK in some contexts.  Note that this function cycles
 * through I64_STR_N_BUFS buffers, so the returned pointers may be
 * valid for that many calls to this function.
 *
 * @param num the signed 64-bit number
 * @return a pointer to the converted C-string. This is a pointer to a static
 *         buffer and is thus unreliable in a threaded context, 
 *         however there are I64_STR_N_BUFS static buffers that this function
 *         cycles through, so if you are careful you can keep the pointer
 *         around for at least I64_STR_N_BUFS calls to this function.
 */
extern const char * i64str (int64 num);
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
extern int float2string(float f, char *dest, int n, int num_decs);


/** The number of static buffers the float2str function cycles through. */
#define FLOAT2STR_N_BUFS 8
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
extern const char * float2str(float f, int num_decs);
#endif
