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

#ifndef _SHM_MGR_H
#  define _SHM_MGR_H


#include "common.h"
#include "exception.h"
#include "comedi_device.h"
#include "shared_stuff.h"

namespace ShmMgr 
{
  /* TODO: Implement more functionality? Perhaps see about using
           some abstractions to represent comedi channels/gain/range?*/

  extern SharedMemStruct *shm; /* the actual shared memory region */

  extern const char *devFileName; /* the file name of the mbuff character 
				     device we are using */

  void check(); /* inline version */
  void _check(); /* real meat of above... attached to shm or throws exception 
		    on error */
  void detach(); 
  bool attach(); /* returns true iff we sucessfully attached to the shm 
		    or were already attached */

  enum FailureReason {
    Unknown = 0,
    RegionNotFound,
    WrongStructVersion,
    CharacterDevAccessError
  };

  extern FailureReason failureReason;

  const char *failureMessage(); /* returns a string relating 
				   to any errors encountered when
				   trying to attach to the Shm */

  const RTPException & failureException() ; /* Same as above method, but
					       give you the exception
					       instead */

  bool shdMemExists(); /* true iff rt_process.o is loaded and 
			  the mbuff is accessible */

  bool devFileIsValid(); /* returns false if /dev/mbuff is invalid,
			    not found, or unopenable */
			    
  
  /* channel-specific stuff--basically wrappers to CR_PACK */

  /* below returns range index from comedi - use with comedi_get_range */
  uint channelRange(ComediSubDevice::SubdevType t, 
			    uint chan);   
  uint channelRange(int subdevtype, uint chan);
  void setChannelRange(ComediSubDevice::SubdevType t, uint chan, 
		       uint r); 
  void setChannelRange(int subdevtype, uint chan, uint r);
 

  /* is channel controls */
  bool isChanOn(ComediSubDevice::SubdevType t, uint chan);
  bool isChanOn(int t, uint chan); 
  void setChannel (ComediSubDevice::SubdevType t, uint chan, 
		   bool onoroff);
  void setChannel (int t, uint chan, bool onoroff);  
  uint numChannels (ComediSubDevice::SubdevType t);
  uint numChannels (int subdevtype);
  uint numChannelsInUse (ComediSubDevice::SubdevType t);
  uint numChannelsInUse (int subdevtype);

  /* return the right array for this type */
  uint *chanArray(ComediSubDevice::SubdevType t);
  uint *chanArray(int subdevtype);
  char *chanUseArray(ComediSubDevice::SubdevType t);
  char *chanUseArray(int t);


  void clearSpikeSettings();
  void setSpikePolarity(uint chan, SpikePolarity polarity);
  void setSpikeEnabled(uint chan, bool onoroff);
  void setSpikeThreshold(uint chan, double threshold);  
  void setSpikeBlanking(uint chan, uint milliseconds);
  SpikePolarity spikePolarity(uint chan); 
  bool spikeEnabled(uint chan);
  double spikeThreshold(uint chan);
  uint spikeBlanking(uint chan);

  /* other shared memory wrappers... */
  uint samplingRateHz();
  scan_index_t scanIndex();
  uint aiFifoMinor();
  uint spikeFifoMinor();
};

inline 
void 
ShmMgr::check() /* inline version */
{ 
  if (shm == NULL) _check(); 
} 

inline
uint *
ShmMgr::chanArray(int subdevtype) 
{ 
  check();
  return (subdevtype == COMEDI_SUBD_AO 
	  ? shm->ao_chan
	  : shm->ai_chan);
}
 
inline
uint *
ShmMgr::chanArray(ComediSubDevice::SubdevType t) 
{ return chanArray(ComediSubDevice::sd2int(t)); }

 
inline
char *
ShmMgr::chanUseArray (int channeltype) 
{
  check();
  return ( channeltype == COMEDI_SUBD_AO
	   ? shm->ao_chans_in_use
	   : shm->ai_chans_in_use );
}

 
inline
char *
ShmMgr::chanUseArray (ComediSubDevice::SubdevType channeltype) 
{ return chanUseArray(ComediSubDevice::sd2int(channeltype)); }   


inline
uint
ShmMgr::channelRange(ComediSubDevice::SubdevType subdev, uint chan) 
{ return channelRange(ComediSubDevice::sd2int(subdev), chan); }  

 
inline
uint 
ShmMgr::channelRange(int subdevtype, uint chan) 
{
  return CR_RANGE (chanArray(subdevtype)[chan]);
}

 
inline
void 
ShmMgr::setChannelRange(ComediSubDevice::SubdevType s, uint c, 
			uint r)
{setChannelRange(ComediSubDevice::sd2int(s), c, r);}

 
inline
void 
ShmMgr::setChannelRange(int s, uint c, uint r)
{
  uint *arr = chanArray(s);
  arr[c] = CR_PACK(CR_CHAN(arr[c]), r, CR_AREF(arr[c]));
}


inline
bool
ShmMgr::isChanOn (int t, uint c)
{ check(); return is_chan_on(c, chanUseArray(t)) == 1; }

inline
bool
ShmMgr::isChanOn (ComediSubDevice::SubdevType t, uint c) 
{ return isChanOn(ComediSubDevice::sd2int(t), c); }



inline
void
ShmMgr::setChannel(ComediSubDevice::SubdevType t,  uint c, bool onoff)
{ setChannel(ComediSubDevice::sd2int(t), c, onoff); }


inline
void
ShmMgr::setChannel(int t,  uint c, bool onoff)
{ set_chan ( c, chanUseArray(t), onoff ); }

inline
uint
ShmMgr::numChannels(ComediSubDevice::SubdevType t)
{
  return numChannels(ComediSubDevice::sd2int(t));
}

inline
uint 
ShmMgr::numChannels(int t)
{
  check();
  return (t == COMEDI_SUBD_AO 
	  ? shm->n_ao_chans
	  : shm->n_ai_chans);
}

inline
uint
ShmMgr::numChannelsInUse(ComediSubDevice::SubdevType t)
{
  return numChannelsInUse(ComediSubDevice::sd2int(t));
}

inline
uint
ShmMgr::numChannelsInUse(int t)
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
ShmMgr::samplingRateHz()
{
  check();
  return shm->sampling_rate_hz;
}

inline 
scan_index_t
ShmMgr::scanIndex()
{
  check();
  return shm->scan_index;
}

inline uint ShmMgr::aiFifoMinor() { check(); return shm->ai_fifo_minor; };
inline uint ShmMgr::spikeFifoMinor() { check(); return shm->spike_fifo_minor; };

#endif
