/*
 * This file is part of the RT-Linux Multichannel Data Acquisition System
 *
 * Copyright (C) 1999, David Christini
 * Copyright (C) 2001, Calin Culianu
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

#ifndef __FIFOS_H
# define __FIFOS_H

/*----------------------------------------------------------------------------
  openFifos
    Opens all the needed device file FIFO's (/dev/rtf*).  
 
  PARAMETERS
   readOnlyDevNumbers 
     - Pass an  array of ints to indicate which FIFO 
       device files to open for reading (eg: {0,7,3} would open 
       /dev/rtf0, /dev/rtf7, and /dev/rtf3, in that order.
   readOnlyArrSize
     - The size of the above array
   writeOnlyDevNumbers    
     - Pass an  array of ints to indicate which FIFO 
       device files to open for writing (eg: {0,7,3} would open
       /dev/rtf0, /dev/rtf7, and /dev/rtf3, in that order.
   writeOnlyArrSize
     - The size of the above array
   readFileDescs
     - The place to put the new read-only file descriptors.  This array 
       whould be of pre-allocated size readOnlyArrSize
   writeFileDescs
     - The place to put the new write-only file descriptors.  This array 
       whould be of pre-allocated size writeOnlyArrSize

       This auto-dies with a message on error.
----------------------------------------------------------------------------*/
extern
void 
openFifos
(
 const unsigned int readOnlyDevNumbers[],
                                  /* the 'set' of /dev/rtf files to open
				     as read-only                           */
 size_t readOnlyArrSize,          /* the size of int readOnlyDevNumbers[]   */
 const unsigned int writeOnlyDevNumbers[], 
                                  /* currently unused                       */
 size_t writeOnlyArrSize,         /* currently unused                       */
 int readFileDescs[],             /* place to put the new read descriptors  */
 int writeFileDescs[]             /* place to put the new write descriptors */
);

#endif
