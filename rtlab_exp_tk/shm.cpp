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
#include "shm.h"
#include "exception.h"
#include "shared_stuff.h"
#include <qobject.h>
#include <qstring.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/major.h>


#include "tweaked_mbuff.h"

/* note that these inlines mean stuff gets inlined only within
   this .o file, but elsewhere in the app it isn't inlined! */


ShmController::ShmController(SharedMemStruct *shm) : shm(shm) {}
ShmController::~ShmController() {};

void ShmController::clearSpikeSettings()
{
  init_spike_params(&shm->spike_params);
}

void ShmController::setSpikePolarity(uint chan, SpikePolarity p) 
{
  _set_bit(chan, shm->spike_params.polarity_mask, p);
}

void ShmController::setSpikeEnabled(uint chan, bool onoroff)
{
  _set_bit(chan, shm->spike_params.enabled_mask, onoroff);
}

void ShmController::setSpikeThreshold(uint chan, double d)
{
  shm->spike_params.threshold[chan] = d;
}

void ShmController::setSpikeBlanking(uint chan, uint msec)
{
  shm->spike_params.blanking[chan] = msec;
}

SpikePolarity ShmController::spikePolarity(uint chan)  const 
{
  return (SpikePolarity)_test_bit(chan, shm->spike_params.polarity_mask);
}

bool ShmController::spikeEnabled(uint chan) const
{
  return _test_bit(chan, shm->spike_params.enabled_mask);
}

double ShmController::spikeThreshold(uint chan) const
{
  return shm->spike_params.threshold[chan];
}

uint ShmController::spikeBlanking(uint chan) const
{
  return shm->spike_params.blanking[chan];
}



ShmBase::FailureReason ShmBase::failureReason = ShmBase::Unknown;

const char * ShmBase::failureReasons[] = {
  "There is no error.",

  "The cause of the error is unknown.",
  
  "It is likely that rt_process.o (or the comedi coprocess) is not loaded, "
  "because the shared memory segment could not be found.  If rt_process.o "
  "(or the comedi coprocess) is loaded, check to make sure it is the "
  "correct version for this program.",
  
  "The compiled version of rt_process.o (or the comedi coprocess) and this "
  "program do not match.",

  "The shared memory segment region exported by rt_process.o and/or the "
  "comedi coprocess has the wrong size.  This is an internal error that "
  "should never occur.  Email the programmers!",

  "There was an error accessing the mbuff character device.  Possible "
  "reasons include permissions problems on the character device "
  "and/or the mbuff.o kernel module not being loaded.",

  0
};

ShmException ShmBase::failureException =
  ShmException(QObject::tr("No exception occurred"), 
               QObject::tr("What is the sound of one hand clapping?"));


const char *RTPShm::devFileName = MBUFF_DEV_NAME;


void
ShmBase::operator delete(void *mem)
{
  switch (reinterpret_cast<SharedMemStruct *>(mem)->reserved[0]) {
  case RTP:
    delete reinterpret_cast<RTPShm *>(mem);
    break;    
  case IPC:
    delete reinterpret_cast<IPCShm *>(mem);    
    break;
  default:
    delete reinterpret_cast<SharedMemStruct *>(mem);
    break;
  }  
}

void * 
RTPShm::operator new(size_t size)
{
  static const QString fullErrorMessage 
    (QObject::tr("Could not attach to rt_process's shared memory segment "
                 "named:\"") +
     + SHARED_MEM_NAME 
     + QObject::tr("\" via the RTL 'mbuff.o' driver (accessed through ") +
     "%1).");

  if (size < sizeof(SharedMemStruct)) size = sizeof(SharedMemStruct);

  void *mem = mbuff_attach(SHARED_MEM_NAME, size);
  SharedMemStruct * & shm = reinterpret_cast<SharedMemStruct *>(mem);

  if (shm && shm->struct_version != SHD_SHM_STRUCT_VERSION) {
    failureReason = WrongStructVersion;
    mbuff_detach(SHARED_MEM_NAME, shm); 
    shm = NULL;
  } else if ( !devFileIsValid() ) {
    failureReason = CharacterDevAccessError;
    shm = NULL;
  }

  failureException =
    ShmException(QObject::tr("Could not attach to ") + SHARED_MEM_NAME,
                 fullErrorMessage.arg(devFileName) + "\n\n" 
                 + failureReasons[failureReason], Exception::GUI);

  if (!shm) throw failureException; 
  
  shm->reserved[0] = RTP; /* now put our rtti stiff in there */
  
  return mem;
}

bool /* mbuff file exists and is readable */
RTPShm::devFileIsValid() 
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

void
RTPShm::operator delete(void *mem)
{
  mbuff_detach(SHARED_MEM_NAME, mem);
}

# include <sys/types.h>
# include <sys/shm.h>

void *
IPCShm::operator new(size_t size)
{
  struct shmid_ds shmid_ds;

  static const QString fullErrorMessage 
    (QObject::tr("Could not attach to the interprocess shared memory segment "
                 "named:\"") +
     + SHARED_MEM_NAME 
     + QObject::tr("\" via the comedi coprocess."));

  if (size < sizeof(SharedMemStruct)) size = sizeof(SharedMemStruct);

  int shmid = 0;

  for (uint i = 0; i < strlen(SHARED_MEM_NAME); i++) 
    shmid += reinterpret_cast<const char *>(SHARED_MEM_NAME)[i];
  

  void *mem = shmat(shmid, 0, 0);
  SharedMemStruct * & shm = reinterpret_cast<SharedMemStruct *>(mem);
  
  if (reinterpret_cast<int>(shm) == -1) {
    failureReason = RegionNotFound;
    shm = NULL;
  }

  if (shm && shm->struct_version != SHD_SHM_STRUCT_VERSION) {
    failureReason = WrongStructVersion;
    shmdt(shm); 
    shm = NULL;
  } else if (shm && !shmctl(shmid, IPC_STAT, &shmid_ds) && shmid_ds.shm_segsz < size) {
    shmdt(shm);
    failureReason = WrongStructSize;
    shm = NULL;
  }

  
  failureException =
      ShmException(QObject::tr("Could not attach to ") + SHARED_MEM_NAME,
                   fullErrorMessage + "\n\n" + failureReasons[failureReason], 
                   Exception::GUI);

  if (!shm) throw failureException; // change this exception type!
  
  shm->reserved[0] = IPC; /* now put our rtti stiff in there */ 

  return mem;
}

void
IPCShm::operator delete (void * mem)
{
  shmdt(mem);
}
