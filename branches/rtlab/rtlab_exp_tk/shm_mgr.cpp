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
#include <mbuff.h>


SharedMemStruct *ShmMgr::shm = NULL;

void 
ShmMgr::check()
{
  static const char * 
    fullErrorMessage = 
      "Could not attach to rt_process's shared memory segment named:\""
      SHARED_MEM_NAME "\"\n"
      "via the RTL mbuff driver.  Either there is an error with the driver, "
      "rt_process.o is not loaded, or the compiled version of rt_process.o "
      "and\nthis program do not match.\n";

  if (!shm) {
    shm = (SharedMemStruct *) mbuff_attach(SHARED_MEM_NAME, 
						    sizeof(SharedMemStruct));
    
    if (shm == NULL || shm->struct_version != SHD_SHM_STRUCT_VERSION) {
      throw RTPException(QString("Could to attach to ") + SHARED_MEM_NAME,
			 fullErrorMessage, Exception::GUI);
      
    }
    /* Just to make sure, we tell rt_process to turn off all the ai channels 
       when we first attach, just as a convenience/convention */
    for (unsigned int i = 0; i < shm->n_ai_chans; i++) {
      setChannel(ComediSubDevice::AnalogInput, i, false);
    }

  }

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

unsigned int *
ShmMgr::chanArray(int subdevtype) 
{ 
  check();
  return (subdevtype == COMEDI_SUBD_AO 
	  ? shm->ao_chan
	  : shm->ai_chan);
}
 
unsigned int *
ShmMgr::chanArray(ComediSubDevice::SubdevType t) 
{ return chanArray(ComediSubDevice::sd2int(t)); }

 
char *
ShmMgr::chanUseArray (int channeltype) 
{
  check();
  return ( channeltype == COMEDI_SUBD_AO
	   ? shm->ao_chans_in_use
	   : shm->ai_chans_in_use );
}

 
char *
ShmMgr::chanUseArray (ComediSubDevice::SubdevType channeltype) 
{ return chanUseArray(ComediSubDevice::sd2int(channeltype)); }   


unsigned int
ShmMgr::channelRange(ComediSubDevice::SubdevType subdev, unsigned int chan) 
{ return channelRange(ComediSubDevice::sd2int(subdev), chan); }  

 
unsigned int 
ShmMgr::channelRange(int subdevtype, unsigned int chan) 
{
  return CR_RANGE (chanArray(subdevtype)[chan]);
}

 
void 
ShmMgr::setChannelRange(ComediSubDevice::SubdevType s, unsigned int c, 
			unsigned int r)
{setChannelRange(ComediSubDevice::sd2int(s), c, r);}

 
void 
ShmMgr::setChannelRange(int s, unsigned int c, unsigned int r)
{
  unsigned int *arr = chanArray(s);
  arr[c] = CR_PACK(CR_CHAN(arr[c]), r, CR_AREF(arr[c]));
}


bool
ShmMgr::isChanOn (int t, unsigned int c)
{ return is_chan_on(c, chanUseArray(t)) == 1; }


bool
ShmMgr::isChanOn (ComediSubDevice::SubdevType t, unsigned int c) 
{ return isChanOn(ComediSubDevice::sd2int(t), c); }



void
ShmMgr::setChannel(ComediSubDevice::SubdevType t,  unsigned int c, bool onoff)
{ setChannel(ComediSubDevice::sd2int(t), c, onoff); }


void
ShmMgr::setChannel(int t,  unsigned int c, bool onoff)
{ set_chan ( c, chanUseArray(t), onoff ); }
