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
#include <sys/types.h>
#include <sys/shm.h>


#include "tweaked_mbuff.h"

const QString ShmController::failureReasons[] = {
  QObject::tr("There is no error."),

  QObject::tr("The cause of the error is unknown."),
  
  QObject::tr(
  "It is likely that rt_process.o (or the comedi coprocess) is not loaded, "
  "because the shared memory segment could not be found.  If rt_process.o "
  "(or the comedi coprocess) is loaded, check to make sure it is the "
  "correct version for this program."),
  
  QObject::tr(
  "The compiled version of rt_process.o (or the comedi coprocess) and this "
  "program do not match."),

  QObject::tr(
  "The shared memory segment region exported by rt_process.o and/or the "
  "comedi coprocess has the wrong size.  This is an internal error that "
  "should never occur.  Email the programmers!"),

  QObject::tr(
  "There was an error accessing the mbuff character device.  Possible "
  "reasons include permissions problems on the character device "
  "and/or the mbuff.o kernel module not being loaded."),

  0
};


ShmController::ShmController(ShmType t) 
  throw(ShmException, /* if cannot attach to type */
        IllegalStateException /* if t is Unknown */
        )
{
  shm = attach(t);
  shmType = t;
}

/* static */
SharedMemStruct * 
ShmController::attach(ShmType t)
{
  SharedMemStruct *shm = 0;
  FailureReason failureReason = Ok;
  QString fullErrorMessage;
  struct shmid_ds shmid_ds;
  int shmid = 0;


  switch (t) {
  case MBuff:
    fullErrorMessage = 
      QObject::tr("Could not attach to rt_process's shared memory segment "
                  "named:\"") +
      + SHARED_MEM_NAME 
      + QObject::tr("\" via the RTL 'mbuff.o' driver (accessed through "
                    MBUFF_DEV_NAME ")");


    shm = reinterpret_cast<SharedMemStruct *>(mbuff_attach(SHARED_MEM_NAME, 
                                                           sizeof(SharedMemStruct)));

    if (shm && shm->struct_version != SHD_SHM_STRUCT_VERSION) {
      failureReason = WrongStructVersion;
      mbuff_detach(SHARED_MEM_NAME, shm); 
      shm = NULL;
    } else if ( !mbuffDevFileIsValid() ) {
      failureReason = CharacterDevAccessError;
      shm = NULL;
    }   
    break;
  case IPC:
    fullErrorMessage =
    (QObject::tr("Could not attach to the interprocess shared memory segment "
                 "named:\"") +
     + SHARED_MEM_NAME 
     + QObject::tr("\" via the comedi coprocess."));
    for (uint i = 0; i < strlen(SHARED_MEM_NAME); i++) 
      shmid += reinterpret_cast<const char *>(SHARED_MEM_NAME)[i];
    

    shm = reinterpret_cast<SharedMemStruct *>( shmat(shmid, 0, 0) );
        
    if (!shm || reinterpret_cast<int>(shm) == -1) {
      failureReason = RegionNotFound;
      shm = NULL;
      break;
    }

    if (shm->struct_version != SHD_SHM_STRUCT_VERSION) {
      failureReason = WrongStructVersion;
      shmdt(shm); 
      shm = NULL;
    } else if (!shmctl(shmid, IPC_STAT, &shmid_ds) 
               && shmid_ds.shm_segsz < sizeof(SharedMemStruct)) {
      shmdt(shm);
      failureReason = WrongStructSize;
      shm = NULL;
    }
    break;
  default:
    throw IllegalStateException("Illegal State in ShmController::attach()",
                                "INTERNAL BUG: ShmController::attach() was "
                                "given an invalid argument.");
    break;
  } /* end switch */

  if (!shm) throw ShmException ( QObject::tr("Could not attach to ") 
                                 + SHARED_MEM_NAME,
                                 fullErrorMessage + "\n\n" 
                                 + failureReasons[failureReason] );

  return shm;                                
}

ShmController::ShmController(SharedMemStruct *shm) 
  : shm(shm), 
    shmType(NotSharedOrUnknown) 
{
  /* nothing.. */
}

ShmController::~ShmController() 
{
  detach(shm, shmType);
}

/* static */
void 
ShmController::detach (SharedMemStruct *shm, ShmType t)
{
  switch (t) {
  case MBuff:
    mbuff_detach(SHARED_MEM_NAME, shm);
    break;
  case IPC:
    shmdt(shm);
    break;
  default:
    /* nothing.. */
    break;
  }
}

/* static */
bool /* mbuff file exists and is readable */
ShmController::mbuffDevFileIsValid() 
{
  /* since mbuff_attach isn't very good about giving us exactly what 
     went wrong, we will attempt to figure out if it was a permissions
     issue or some other error. */
  struct stat statbuf;
  int fd = -1;
  
  if (stat (MBUFF_DEV_NAME, &statbuf) || !S_ISCHR(statbuf.st_mode)
      || (fd = open(MBUFF_DEV_NAME, O_RDWR)) < 0 || close(fd)) {
    return false;
  }
  return true;
}


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
