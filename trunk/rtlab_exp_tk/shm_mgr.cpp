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
#include "common.h"
#include "shm_mgr.h"
#include "exception.h"
#include "shared_stuff.h"
#include <qobject.h>
#include <qstring.h>
#include <mbuff.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/major.h>

SharedMemStruct *ShmMgr::shm = NULL;
const char *ShmMgr::devFileName = MBUFF_DEV_NAME;
ShmMgr::FailureReason ShmMgr::failureReason = ShmMgr::Unknown;

static const QString failureReasons[] = {
  QObject::
  tr("The cause of the error is unknown."),
  
  QObject::
  tr("It is likely that rt_process.o is not loaded, because the shared "
	 "memory segment could not be found.  If rt_process.o is loaded, "
	 "check to make sure it is the correct version for this program."),
  
  QObject::
  tr("The compiled version of rt_process.o and this program do not match."),

  QObject::
  tr("There was an error accessing the mbuff character device.  Possible "
	 "reasons include permissions problems on the character device "
	 "and/or the mbuff.o kernel module not being loaded."),
  0
};

static RTPException failure_exception = 
  RTPException(QObject::tr("No exception occurred"), 
	       QObject::tr("What is the sound of one hand clapping?"));

/* note that these inlines mean stuff gets inlined only within
   this .o file, but elsewhere in the app it isn't inlined! */


void 
ShmMgr::_check()
{
  static const QString fullErrorMessage 
    (QObject::tr("Could not attach to rt_process's shared memory segment "
		 "named:\"") +
     + SHARED_MEM_NAME 
     + QObject::tr("\" via the RTL 'mbuff.o' driver (accessed through ") +
     "%1).");


  failureReason = Unknown;   

    
  if (shdMemExists()) {
      
    shm = (SharedMemStruct *) mbuff_attach(SHARED_MEM_NAME, 
					   sizeof(SharedMemStruct));
    
    if (shm && shm->struct_version != SHD_SHM_STRUCT_VERSION) {
      failureReason = WrongStructVersion;
      mbuff_detach(SHARED_MEM_NAME, shm); 
      shm = NULL;
    }
    
  } else {
    
    failureReason = (devFileIsValid()
		     ? RegionNotFound 
		     : CharacterDevAccessError);
    
  }
  
  if (shm == NULL) {
    failure_exception =
      RTPException(QObject::tr("Could not attach to ") + SHARED_MEM_NAME,
		   fullErrorMessage.arg(devFileName) + "\n\n" +
		   failureReasons[failureReason], 
		   Exception::GUI);
    throw failure_exception;
  }
    
  /* If we get to this point, SUCCESS! :) */

  /* Just to make sure, we tell rt_process to turn off all the ai channels 
       when we first attach, just as a convenience/convention */
  for (uint i = 0; i < shm->n_ai_chans; i++) {
    setChannel(ComediSubDevice::AnalogInput, i, false);
  }
  
  /* reset the scan index so that we start with a 'clean slate' for
     this acquisition session.  To Do: revise this approach?  */
  shm->scan_index = 0;
  
}

void
ShmMgr::detach()
{
  if (shm) { 
    mbuff_detach(SHARED_MEM_NAME, shm); 
    shm = NULL; 
  }
}

bool /* returns true iff we sucessfully attached to the shm */
ShmMgr::attach()
{
  try {
    check();
  } catch (RTPException & e) {
    return false;
  }
  return true;
}

/* returns a string relating to any errors encountered when trying to attach 
   to the Shm */
const char *
ShmMgr::failureMessage() 
{
  return failureReasons[failureReason];
}

/* Same as above method, but give you the exception instead */
const RTPException &
ShmMgr::failureException() 
{
  return failure_exception;
}

bool /* true iff rt_process.o is loaded and the mbuff is accessible */
ShmMgr::shdMemExists() 
{
  bool ret = false;
  int fd;
  struct mbuff_request_struct req;

  if (!devFileIsValid()) {
    goto end;
  }
  
  fd = open(devFileName, O_RDWR);
  strncpy(req.name, SHARED_MEM_NAME, MBUFF_NAME_LEN);
  req.name[MBUFF_NAME_LEN+1] = 0;
  ret = ioctl(fd, IOCTL_MBUFF_SELECT, &req) >= 0;
  close(fd);
 end:
  return ret;
}

bool /* mbuff file exists and is readable */
ShmMgr::devFileIsValid() 
{
  /* since mbuff_attach isn't very good about giving us exactly what 
     went wrong, we will attempt to figure out if it was a permissions
     issue or some other error. */
  struct stat statbuf;
  int fd = -1;
  
  if (stat (devFileName, &statbuf) || !S_ISCHR(statbuf.st_mode)
      || (fd = open(devFileName, O_RDWR)) < 0 || close(fd)) {
    return false;
  }
  return true;
}


void ShmMgr::clearSpikeSettings()
{
  check();
  init_spike_params(&shm->spike_params);
}

void ShmMgr::setSpikePolarity(uint chan, SpikePolarity p) 
{
  check();
  _set_bit(chan, shm->spike_params.polarity_mask, p);
}

void ShmMgr::setSpikeEnabled(uint chan, bool onoroff)
{
  check();
  _set_bit(chan, shm->spike_params.enabled_mask, onoroff);
}

void ShmMgr::setSpikeThreshold(uint chan, double d)
{
  check();
  shm->spike_params.threshold[chan] = d;
}

void ShmMgr::setSpikeBlanking(uint chan, uint msec)
{
  check();
  shm->spike_params.blanking[chan] = msec;
}

SpikePolarity ShmMgr::spikePolarity(uint chan) 
{
  check();
  return (SpikePolarity)_test_bit(chan, shm->spike_params.polarity_mask);
}

bool ShmMgr::spikeEnabled(uint chan)
{
  check();
  return _test_bit(chan, shm->spike_params.enabled_mask);
}

double ShmMgr::spikeThreshold(uint chan)
{
  check();
  return shm->spike_params.threshold[chan];
}

uint ShmMgr::spikeBlanking(uint chan)
{
  check();
  return shm->spike_params.blanking[chan];
}



