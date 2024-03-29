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
#include <qfile.h>
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

#include "user_to_kernel.h"

#define RTO RTLAB_MODULE_NAME ".o" 

const QString ShmController::failureReasons[] = {
  QObject::tr("There is no error."),

  QObject::tr("The cause of the error is unknown."),
  
  QObject::tr(
  "It is likely that " RTO " (or the comedi coprocess) is not loaded, "
  "because the shared memory segment could not be found.\nIf " RTO 
  "(or the comedi coprocess) is loaded, check to make sure it is the "
  "correct version for this program."),
  
  QObject::tr(
  "The compiled version of " RTO " (or the comedi coprocess) and this "
  "program do not match."),

  QObject::tr(
  "The shared memory segment region exported by " RTO " and/or the "
  "comedi coprocess has the wrong size.  This is an internal error that "
  "should never occur.  Email the programmers!"),

  QObject::tr(
  "There was an error accessing the mbuff character device.  Possible "
  "reasons include permissions problems on the character device "
  "and/or the mbuff.o kernel module not being loaded."),

  0
};


ShmController::ShmController(const SharedMemStruct *s) 
  throw(ShmException, IllegalStateException)
  : shm(s)
{
  if (s) {
    /* it's already attached so do very little.. */
    shmType = NotSharedOrUnknown;
    return;
  }

  static const int types2try_sz = 3;
  ShmType types2try[types2try_sz] = {MBuff, RTAI_Shm, IPC};
  ShmException exception;

  for (int i = 0; !shm && i < types2try_sz; i++) {
    ShmType t = types2try[i];
    
    try {
      shm = attach(t);
      shmType = t;
    } catch (ShmException & e) {
      shm = 0;
      /* gracefully ignore .. */
    } 
  }

  if (!shm) throw exception;
}


/* static */
const SharedMemStruct * 
ShmController::attach(ShmType t)
{
  const SharedMemStruct *shm = 0;
#define shm_notype (reinterpret_cast<void *>(const_cast<SharedMemStruct *>(shm)))
  FailureReason failureReason = Ok;
  QString fullErrorMessage;
  struct shmid_ds shmid_ds;
  int shmid = 0;


  switch (t) {
  case MBuff:    
    fullErrorMessage = 
      QObject::tr("Could not attach to " RTLAB_MODULE_NAME 
                  "'s shared memory segment named:\"") +
      + SHARED_MEM_NAME 
      + QObject::tr("\" via the RTL 'mbuff.o' driver (accessed through "
                    MBUFF_DEV_NAME ")");

    if (!haveRTLabDotO()) {
      failureReason = RegionNotFound;
      shm_notype = NULL;
      break;
    }

    shm_notype = mbuff_attach(SHARED_MEM_NAME, sizeof(SharedMemStruct));

    if (shm && shm->struct_version != SHD_SHM_STRUCT_VERSION) {
      failureReason = WrongStructVersion;
      mbuff_detach(SHARED_MEM_NAME, shm_notype); 
      shm_notype = NULL;
    } else if ( !mbuffDevFileIsValid() ) {
      failureReason = CharacterDevAccessError;
      shm_notype = NULL;
    }   
    break;
  case RTAI_Shm:
    fullErrorMessage = 
      QObject::tr("Could not attach to rtlab's shared memory segment "
                  "named:\"") +
      + SHARED_MEM_NAME 
      + QObject::tr("\" via the RTAI 'rtai_shm.o' driver (accessed through "
                    RTAI_SHM_DEV ")");

    if (!haveRTLabDotO()) {
      failureReason = RegionNotFound;
      shm_notype = NULL;
      break;
    }

    shm_notype = rtai_shm_attach(SHARED_MEM_NAME, sizeof(SharedMemStruct));

    if (shm && shm->struct_version != SHD_SHM_STRUCT_VERSION) {
      failureReason = WrongStructVersion;
      rtai_shm_detach(SHARED_MEM_NAME, shm_notype); 
      shm_notype = NULL;
    } else if ( !rtaiShmDevFileIsValid() ) {
      failureReason = CharacterDevAccessError;
      shm_notype = NULL;
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
    

    shm_notype = shmat(shmid, 0, 0);
        
    if (!shm || reinterpret_cast<int>(shm) == -1) {
      failureReason = RegionNotFound;
      shm_notype = NULL;
      break;
    }

    if (shm->struct_version != SHD_SHM_STRUCT_VERSION) {
      failureReason = WrongStructVersion;
      shmdt(shm_notype); 
      shm_notype = NULL;
    } else if (!shmctl(shmid, IPC_STAT, &shmid_ds) 
               && shmid_ds.shm_segsz < sizeof(SharedMemStruct)) {
      shmdt(shm_notype);
      failureReason = WrongStructSize;
      shm_notype = NULL;
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
#undef shm_notype
}


ShmController::~ShmController() 
{
  detach(shm, shmType);
}

/* static */
void 
ShmController::detach (const SharedMemStruct *shm, ShmType t)
{
  switch (t) {
  case MBuff:
    mbuff_detach(SHARED_MEM_NAME, (void *)shm);
    break;
  case IPC:
    shmdt(shm);
    break;
  case RTAI_Shm:
    rtai_shm_detach(SHARED_MEM_NAME, (void *)shm); 
    break;
  default:
    IllegalStateException("Illegal State in ShmController::detach()",
                          "INTERNAL BUG: ShmController::detach() was "
                          "given an invalid argument.");
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

/* static */
bool /* rtai_shm file exists and is readable */
ShmController::rtaiShmDevFileIsValid() 
{
  /* since rtai_shm_attach isn't very good about giving us exactly what 
     went wrong, we will attempt to figure out if it was a permissions
     issue or some other error. */
  struct stat statbuf;
  int fd = -1;
  
  if (stat (RTAI_SHM_DEV, &statbuf) || !S_ISCHR(statbuf.st_mode)
      || (fd = open(RTAI_SHM_DEV, O_RDWR)) < 0 || close(fd)) {
    return false;
  }
  return true;
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

bool ShmController::haveRTLabDotO()
{
  return QFile::exists("/proc/" RTLAB_MODULE_NAME );
}


ShmControllerWithFifo::ShmControllerWithFifo()
  : rtlab(0)
{
  rtlab = new RTLabKernelNotifier(shm->control_fifo, shm->reply_fifo);
  /* tell rtlab.o who we are? */
  rtlab->setAttachedPid(getpid());
}

ShmControllerWithFifo::~ShmControllerWithFifo()
{
  rtlab->setAttachedPid(-1);
  if (rtlab) { delete rtlab; rtlab = 0; }
}



void ShmControllerWithFifo::setChannel(int t, uint chan, bool onoroff)
{
  (void)t;
  rtlab->setChan(chan, onoroff);
}

void ShmControllerWithFifo::setChannelRange(int subdevtype, uint chan, uint r)
{
  (void)subdevtype;
  rtlab->setGain(chan, r);
}

void 
ShmControllerWithFifo::setAREFAll(int s, uint a)
{
  (void)s;
  rtlab->setAllAREFs(a);
}


void ShmControllerWithFifo::setChannelAREF(int subdevtype, uint chan, uint a)
{
  (void)subdevtype;
  rtlab->setAREF(chan, a);
}

void ShmControllerWithFifo::clearSpikeSettings()
{
  rtlab->setAllSpikes(false);
  rtlab->setAllSpikePolarities(Negative);
  rtlab->setAllSpikeBlankings(DEFAULT_SPIKE_BLANKING);
  rtlab->setAllSpikeThresholds(0.0);
}

void ShmControllerWithFifo::setSpikePolarity(uint chan, SpikePolarity p) 
{
  rtlab->setSpikePolarity(chan, p);
}

void ShmControllerWithFifo::setSpikeEnabled(uint chan, bool onoroff)
{
  rtlab->setSpike(chan, onoroff);
}

void ShmControllerWithFifo::setSpikeThreshold(uint chan, double d)
{
  rtlab->setSpikeThreshold(chan, d);
}

void ShmControllerWithFifo::setSpikeBlanking(uint chan, uint msec)
{
  rtlab->setSpikeBlanking(chan, msec);
}


uint ShmControllerWithFifo::setSamplingRateHz(uint rate)
{
  return rtlab->setSamplingRate(rate);
}
