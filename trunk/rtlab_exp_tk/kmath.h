/*
 * This file is part of the RT-Linux Multichannel Data Acquisition System
 *
 * Copyright (C) 2003 David Christini
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
#ifndef _KMATH_H
#define _KMATH_H

/** Some helpful kernel math-like functions.  Rounding, sine, etc.  For
    now just round() is here.. */

#ifdef __cplusplus
extern "C" {
#endif

  /** helper rounding function */
  static inline int round (double d) 
  { 
    return (int)(d + (d >= 0 ? 0.5 : -0.5)); 
  }

  static inline long long int llround(double d)
  {
    return (long long)(d + (d >= 0 ? 0.5 : -0.5)); 
  }
#ifdef __cplusplus
}
#endif




#endif
