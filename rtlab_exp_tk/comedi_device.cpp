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

#include <qregexp.h>
#include "comedi_device.h"


const ComediRange::Unit     ComediRange::Volts      = UNIT_volt, 
                            ComediRange::MilliVolts = UNIT_mA;

ComediRange::Unit::Unit(int i) { *this = i; }
bool ComediRange::Unit::operator==(const Unit &in) { return in.u == u; } 
ComediRange::Unit & ComediRange::Unit::operator=(int i) 
    { u = ( i == UNIT_volt ? UNIT_volt : UNIT_mA ); return *this; } 
ComediRange::Unit::operator int() const { return u; }
ComediRange::Unit::operator QString() const
    { return QString( u == UNIT_volt ? "V" : "mV" ); }



/* below returns a ComediSubDevice with isNull()==true if not found */
const ComediSubDevice &
ComediDevice::find(ComediSubDevice::SubdevType type, int id_start) const
{
  static const ComediSubDevice nullSubDevice((float *)0);
  for (uint i = 0; i < subdevices.size(); i++) {
    if (subdevices[i].id >= id_start && subdevices[i].type == type) {
      return subdevices[i];
    }
  }
  /* if it's not found */
  return nullSubDevice;
}

QString
ComediSubDevice::  /* Generates a string for the given range */
generateRangeString(uint rangeId) const
{
  QString ret;

  if (rangeId < ranges().size() ) {
    /* build range id combo box inside here */
    const comedi_range & r = ranges()[rangeId];
    ret = ComediRange::buildString(r.min, r.max, r.unit);
  }

  return ret;
}

QString
ComediRange::
buildString (double min, double max, Unit unit) 
{
  static const QString format ("%1%3 - %2%4");

  QString ustr = static_cast<QString>(unit); // operator QString()
  return format.arg(min, 2).arg(max, 2).arg(ustr).arg(ustr);
}

/** parses the range setting string and places its components in
    the provided reference parameters */
bool
ComediRange::
parseString (const QString & rangeString, 
             double & rangeMin, 
             double & rangeMax, 
             Unit & units) 
{
  static QRegExp regexp("-?[0-9]+\\.?-?[0-9]*");
  int index=0,len=0;
  
  if ( (index = regexp.match(rangeString, 0, &len)) != -1
       && sscanf(rangeString.mid(index,len), "%lf", &rangeMin) 
       && (index = regexp.match(rangeString, index+len, &len)) != -1
       && sscanf(rangeString.mid(index,len), "%lf", &rangeMax) ) {
    // look for the units string
    units = ((rangeString.find((QString)MilliVolts, index + len ) > -1) 
             ? MilliVolts 
             : Volts );
    return true;
  }
  return false;
}

