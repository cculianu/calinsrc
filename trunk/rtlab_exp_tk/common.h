/*
 * This file is part of the RT-Linux Multichannel Data Acquisition System
 *
 * Copyright (C) 1999,2000 David Christini
 * Copyright (c) 2001 David Christini, Lyuba Golub, Calin Culianu
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

#ifndef _COMMON_H
#  define _COMMON_H
#  include "config.h"
#  include <values.h>

typedef unsigned int uint;

typedef short int16;
typedef unsigned short uint16;

typedef char int8;
typedef unsigned char uint8;

typedef unsigned long long uint64;
typedef long long int64;

#include <string>
#include <qstring.h>


bool cstr_to_uint64(const char *in, uint64 & out);
uint64 cstr_to_uint64(const char *in, bool *ok = 0);

/* this function is non-reentrant! */
const char *uint64_to_cstr(uint64 in);


/* this function is non-reentrant! */
string operator+(const string & s, uint64 in);

/* this function is non-reentrant! */
QString operator+(const QString & s, uint64 in);


#endif
