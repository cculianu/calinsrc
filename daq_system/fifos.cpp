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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "fifos.h"

// the prefix of all the /dev/rtf* special devices
static const char * const rtfDeviceFilePrefix = "/dev/rtf";     


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
  RETURNS
    0 on success, an errno value on error.
----------------------------------------------------------------------------*/
int
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
) {

  /* only needs to be computed once -- the length of the string  "/dev/rtf" */
  static int rtfDeviceFilePrefixLen = strlen(rtfDeviceFilePrefix);

  /* filename buffer for dynamically building "/dev/rtfXX" filename         */
  char filenameBuffer[ rtfDeviceFilePrefixLen + 4 ]; 
  int  i,                        /* loop index                              */
       j,                        /* loop index                              */
      *currentDescs,             /* current destination descs               */
       currentMode,              /* current open mode                       */
       currentSize;              /* the size of the array currentDevNums    */
  const unsigned int *currentDevNums;  
                                 /* temp pointer to 
				    [read|write]OnlyDevNumbers              */


  strcpy (filenameBuffer, rtfDeviceFilePrefix);

  for (i = 0; i < 2; i++) {
    if (i==0) {
      /* do the read-only setup */
      currentDevNums = readOnlyDevNumbers;
      currentSize = readOnlyArrSize;
      currentDescs = readFileDescs;
      currentMode = O_RDONLY; 
    } else {
      /* do the write-only setup */
      currentDevNums = writeOnlyDevNumbers;
      currentSize = writeOnlyArrSize;
      currentDescs = writeFileDescs;
      currentMode = O_WRONLY; 
    }
    if (currentDevNums == NULL) {
      continue;
    }

    for (j = 0; j < currentSize; j++) {
      // create the /dev/rtfXX filename string
      sprintf ( 
	       (filenameBuffer + rtfDeviceFilePrefixLen), 
	       "%d", 
	       currentDevNums[j]
	      );
      if ( ( currentDescs[j] = open(filenameBuffer, currentMode) ) < 0 ) {
	char tmpErrStr[512];
	int tmp_errno = errno;
	
	sprintf(
		tmpErrStr, 
		"read-only fifo_index %d: Error opening %s", 
		j, 
		filenameBuffer
	       );
	/* get the error message from errno */
	perror (tmpErrStr);
	return tmp_errno;
      }   
    }
  }
  return 0;
}
