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

#ifndef _SHM_CTL_H
#  define _SHM_CTL_H


#include "common.h"
#include "exception.h"
#include "comedi_device.h"
#include "shared_stuff.h"

class QString;

class ShmController 
{
 public:

  enum ShmType { MBuff, IPC, RTAI_Shm, NotSharedOrUnknown = 0 };

 protected:
  /* if shm is NULL, 
     probes for the right shm type then calls attach() with that type 
     otherwise if shm is not NULL, takes shm as an already attached
     shm and doesn't detatch it upon class destruction
   */
  ShmController(const SharedMemStruct *shm = 0) 
    throw(ShmException, /* if cannot attach to type */
	  IllegalStateException /* if t is Unknown */
	  );
 public:
  virtual ~ShmController();

  static const SharedMemStruct *attach(ShmType t = MBuff);
  static void detach(const SharedMemStruct *, ShmType t = MBuff);

  /* GETTERS */

  /* channel-specific stuff--basically wrappers to CR_PACK */

  /* below returns range index from comedi - use with comedi_get_range */
  uint channelRange(ComediSubDevice::SubdevType t, uint chan) const;   
  uint channelRange(int subdevtype, uint chan) const;
  uint channelAREF(ComediSubDevice::SubdevType t, uint chan) const;
  uint channelAREF(int subdevtype, uint chan) const;

  /* is channel controls */
  bool isChanOn(ComediSubDevice::SubdevType t, uint chan) const;
  bool isChanOn(int t, uint chan) const; 

  uint numChannels (ComediSubDevice::SubdevType t) const;
  uint numChannels (int subdevtype) const;
  uint numChannelsInUse (ComediSubDevice::SubdevType t) const;
  uint numChannelsInUse (int subdevtype) const;

  SpikePolarity spikePolarity(uint chan) const; 
  bool spikeEnabled(uint chan) const;
  double spikeThreshold(uint chan) const;
  uint spikeBlanking(uint chan) const;

  /* other shared memory wrappers... */
  uint samplingRateHz() const;
  scan_index_t scanIndex() const;  
  uint aiFifoMinor() const; /* not meaningful in all contexts */

  /* SETTERS */

  virtual void setChannel (ComediSubDevice::SubdevType t, uint chan, 
			   bool onoroff);
  virtual void setChannel (int t, uint chan, bool onoroff) = 0;  

  virtual void setChannelRange(ComediSubDevice::SubdevType t, uint chan, 
			       uint r); 
  virtual void setChannelRange(int subdevtype, uint chan, uint r) = 0;
  
  /* AREF values should come from comedi.h as the AREF_* set of #defines */
  virtual void setChannelAREF(ComediSubDevice::SubdevType s, uint chan, 
			      uint aref);
  virtual void setChannelAREF(int subdevtype, uint chan, uint aref) = 0;
  virtual void setAREFAll(ComediSubDevice::SubdevType s, uint aref);
  virtual void setAREFAll(int subdevtype, uint aref) = 0;

  /* Spikes.. */
  virtual void clearSpikeSettings() = 0;
  virtual void setSpikePolarity(uint chan, SpikePolarity polarity) = 0;
  virtual void setSpikeEnabled(uint chan, bool onoroff) = 0;
  virtual void setSpikeThreshold(uint chan, double threshold) = 0;  
  virtual void setSpikeBlanking(uint chan, uint milliseconds) = 0;

  /* Dangerous if misused in realtime version of RTLab... */
  virtual uint setSamplingRateHz(uint) = 0;


 protected:
 
  const SharedMemStruct *shm; /* the actual shared memory region */
  
 private:
  
   /* return the right array for this type */
  volatile const uint *chanArray(ComediSubDevice::SubdevType t);
  volatile const uint *chanArray(int subdevtype);
  volatile const char *chanUseArray(ComediSubDevice::SubdevType t);
  volatile const char *chanUseArray(int t);


  static bool haveRTLabDotO(); /* true iff a module named rt_process is 
                                  loaded */
  static bool mbuffDevFileIsValid();
  static bool rtaiShmDevFileIsValid();

  ShmType shmType;

  enum FailureReason {
    Ok = 0, /* no failure */
    Unknown,
    RegionNotFound,
    WrongStructVersion,
    WrongStructSize,
    CharacterDevAccessError
  };
  
  /* above enum is index into this array */
  static const QString failureReasons[];

};

class RTLabKernelNotifier;

class ShmControllerWithFifo : public ShmController
{
 public:
  ShmControllerWithFifo();
  ~ShmControllerWithFifo();

  /* SETTERS */

  void setChannel (int t, uint chan, bool onoroff);  
  void setChannelRange(int subdevtype, uint chan, uint r);
  
  /* AREF values should come from comedi.h as the AREF_* set of #defines */
  void setChannelAREF(int subdevtype, uint chan, uint aref);
  void setAREFAll(int subdevtype, uint aref);

  /* Spikes.. */
  void clearSpikeSettings();
  void setSpikePolarity(uint chan, SpikePolarity polarity);
  void setSpikeEnabled(uint chan, bool onoroff);
  void setSpikeThreshold(uint chan, double threshold);  
  void setSpikeBlanking(uint chan, uint milliseconds);

  // try and set the rate to new_rate and return the new rate 
  uint setSamplingRateHz(uint new_rate);

 private:

  uint controlFifo() const; /* minor of the control fifo */
  
  RTLabKernelNotifier *rtlab;
};

inline
volatile const uint *
ShmController::chanArray(int subdevtype) 
{ 
  return (subdevtype == COMEDI_SUBD_AO 
	  ? shm->ao_chan
	  : shm->ai_chan);
}
 
inline
volatile const uint *
ShmController::chanArray(ComediSubDevice::SubdevType t) 
{ return chanArray(ComediSubDevice::sd2int(t)); }

 
inline
volatile const char *
ShmController::chanUseArray (int channeltype) 
{
  return ( channeltype == COMEDI_SUBD_AO
	   ? shm->ao_chans_in_use
	   : shm->ai_chans_in_use );
}

 
inline
volatile const char *
ShmController::chanUseArray (ComediSubDevice::SubdevType channeltype) 
{ return chanUseArray(ComediSubDevice::sd2int(channeltype)); }   


inline
uint
ShmController::channelRange(ComediSubDevice::SubdevType subdev, uint chan) const
{ return channelRange(ComediSubDevice::sd2int(subdev), chan); }  

 
inline
uint 
ShmController::channelRange(int subdevtype, uint chan) const
{
  return CR_RANGE (const_cast<ShmController *>(this)->chanArray(subdevtype)[chan]);
}

inline
uint 
ShmController::channelAREF(ComediSubDevice::SubdevType t, uint chan) const
{
  return channelAREF(ComediSubDevice::sd2int(t), chan);
}

inline
uint 
ShmController::channelAREF(int subdevtype, uint chan) const
{
  return CR_AREF (const_cast<ShmController *>(this)->chanArray(subdevtype)[chan]);
}

 
inline
void 
ShmController::setChannelRange(ComediSubDevice::SubdevType s, uint c, 
                               uint r)
{setChannelRange(ComediSubDevice::sd2int(s), c, r);}


inline
void 
ShmController::setChannelAREF(ComediSubDevice::SubdevType s, uint c, uint a)
{
  setChannelAREF(ComediSubDevice::sd2int(s), c, a);
}


inline
void 
ShmController::setAREFAll(ComediSubDevice::SubdevType s, uint a)
{
  setAREFAll(ComediSubDevice::sd2int(s), a);
}

inline
bool
ShmController::isChanOn (int t, uint c) const
{ return is_chan_on(c, const_cast<ShmController *>(this)->chanUseArray(t)) == 1; }

inline
bool
ShmController::isChanOn (ComediSubDevice::SubdevType t, uint c) const
{ return isChanOn(ComediSubDevice::sd2int(t), c); }



inline
void
ShmController::setChannel(ComediSubDevice::SubdevType t,  uint c, bool onoff)
{ setChannel(ComediSubDevice::sd2int(t), c, onoff); }


inline
uint
ShmController::numChannels(ComediSubDevice::SubdevType t) const
{
  return numChannels(ComediSubDevice::sd2int(t));
}

inline
uint 
ShmController::numChannels(int t) const
{
  return (t == COMEDI_SUBD_AO 
	  ? shm->n_ao_chans
	  : shm->n_ai_chans);
}

inline
uint
ShmController::numChannelsInUse(ComediSubDevice::SubdevType t) const
{
  return numChannelsInUse(ComediSubDevice::sd2int(t));
}

inline
uint
ShmController::numChannelsInUse(int t) const
{
  uint ret = 0;
  uint n_chans = numChannels(t);

  for (uint i = 0; i < n_chans; i++) {
    ret += isChanOn(t, i);
  }

  return ret;
}

inline
uint
ShmController::samplingRateHz() const
{
  return shm->sampling_rate_hz;
}

inline 
scan_index_t
ShmController::scanIndex() const
{
  return shm->scan_index;
}

inline 
uint 
ShmController::aiFifoMinor() const
{ 
  return shm->ai_fifo_minor; 
}

inline
uint 
ShmControllerWithFifo::controlFifo() const /* minor of the control fifo */
{
  return shm->control_fifo;
}

#endif
