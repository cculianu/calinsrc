/*
 * This file is part of the RT-Linux Multichannel Data Acquisition System
 *
 * Copyright (c) 2001 Calin Culianu
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

#ifndef _COMEDI_DEVICE_H
#define _COMEDI_DEVICE_H

#include <qstring.h>
#include <vector>
#include <comedi.h>
#include <comedilib.h>
#include "shared_stuff.h"
#include "common.h"

namespace ComediChannel
{
  enum AnalogRef {
    Ground = AREF_GROUND,
    Common = AREF_COMMON,
    Differential = AREF_DIFF,
    Other = AREF_OTHER,
    Undefined,
  };
  
  /* helper functions */
  int ar2int(AnalogRef r);
  AnalogRef int2ar(int r);
};

class ComediSubDevice
{  
  friend class ComediDevice;
 public:
  enum SubdevType {
    subdevtype_low = 0,
    AnalogInput,
    AnalogOutput,
    Other,
    Undefined,
    subdevtype_high
  };

  ComediSubDevice(int id = -1, 
		  bool used_by_rt_process = false, 
		  bool can_be_locked = true)   
    : id(id), n_channels(-1), used_by_rt_process(used_by_rt_process), 
      can_be_locked(can_be_locked), type(Undefined), is_null(false) {};

  bool isNull() const { return is_null; };

  vector<comedi_range> & ranges() { return _ranges; }
  const vector<comedi_range> & ranges() const {return _ranges;}

  lsampl_t maxData() const { return maxdata; }
  lsampl_t & maxData() { return maxdata; }

  /*double sampleToUnits(const SampleStruct & sample, 
		       uint *unit = 0) const;
		       
    lsampl_t unitsToSample(SampleStruct & s, double unitdata) const;*/

  /* convert a SubdevType to a native comedi int */
  static int sd2int(SubdevType t);
  /* convert a native comedi int back to a SubdevType */
  static SubdevType int2sd(int i);
  /* note that above two methods aren't perfectly symmetric.. namely
     everything other than COMEDI_SUBD_(AI|AO) get flattened to SubdevType
     'Other', which, when converted back, becomes COMEDI_SUBD_UNUSED */

  int id, n_channels;
  bool used_by_rt_process, can_be_locked;
  SubdevType type; /* from comedi.h */

 private:
  ComediSubDevice(float *dummy) /* dummy constructor for a null dev */
    : is_null(true) { dummy++; };
  bool is_null;
  vector<comedi_range> _ranges;
  lsampl_t maxdata;

};

class ComediDevice 
{  
 public:
  ComediDevice(const QString & filename = "")  
    : filename(filename) {};

  /* returns a null (isNull() == true) ComediSubDevice if not found */
  const ComediSubDevice & find(ComediSubDevice::SubdevType type, 
                               int id_start = 0) const; 

  QString filename;
  QString devicename, drivername;
  int minor; /* from comedi */
  vector<ComediSubDevice> subdevices;
  
  bool isNull() const { return subdevices.size() <= 0; };
};

#define _cd_undefinedval (COMEDI_SUBD_UNUSED-1)
inline
int
ComediSubDevice::sd2int(SubdevType t) 
{ 
  register int ret;
  switch (t) {
  case AnalogInput:
    ret = COMEDI_SUBD_AI;
    break;
  case AnalogOutput:
    ret = COMEDI_SUBD_AO;
    break;
  case Undefined:
    ret = _cd_undefinedval;
    break;
  default:
    ret = COMEDI_SUBD_UNUSED;
    break;
  }
  return ret;
}

inline
ComediSubDevice::SubdevType
ComediSubDevice::int2sd(int i) 
{
  register SubdevType ret;
  switch (i) {
  case COMEDI_SUBD_AI:
    ret = AnalogInput;
    break;
  case COMEDI_SUBD_AO:
    ret = AnalogOutput;
    break;
  case _cd_undefinedval:
    ret = Undefined;
    break;
  default:
    ret = Other;
    break;
  }
  return ret;
}
#undef _cd_undefinedval

#define _ar_undefinedval (static_cast<int>(ComediChannel::Undefined))
inline
int
ComediChannel::ar2int(AnalogRef r) 
{ 
  switch (r) {
  case Ground:
    return AREF_GROUND;
  case Common:
    return AREF_COMMON;
  case Differential:
    return AREF_DIFF;
  case Other:
    return AREF_OTHER;
  default:
    return _ar_undefinedval;
  }
  return _ar_undefinedval; /* not reached */
}

inline
ComediChannel::AnalogRef
ComediChannel::int2ar(int i) 
{
  switch (i) {
  case AREF_GROUND:
    return Ground;
  case AREF_COMMON:
    return Common;
  case AREF_DIFF:
    return Differential;
  case AREF_OTHER:
    return Other;
  default:
    return Undefined;
  }
  return Undefined; /* not reached */
}
#undef _ar_undefinedval


/* pass optional int * to save the unit type in the param 

this is no longer needed?
inline
double 
ComediSubDevice::
sampleToUnits(const SampleStruct &s, uint *unit = 0) const   
{ 
  if (unit) { *unit = _ranges[s.range].unit; }
  return (_ranges[s.range].max - _ranges[s.range].min) * s.data/(double)maxdata
         + _ranges[s.range].min;
}

*/


#endif
