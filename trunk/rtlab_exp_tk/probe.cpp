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
#include <comedi.h>
#include <comedilib.h>
#include <qstring.h>
#include <vector>
#include <iostream>
using namespace std;
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "probe.h"
#include "exception.h"
#include "comedi_device.h"
#include "shm.h"

#ifdef TEST_PROBE
QString
pluralize (const QString & s, int qty);

int
main (void)
{
  try {
    Probe p;

    cout << "RT Process is " << (p.have_rt_process ? "" : "NOT ") 
	 << "loaded..." << endl;

    for (uint i = 0; i < p.probed_devices.size(); i++) {
      ComediDevice & dev = p.probed_devices[i];
      cout << dev.filename << " (minor " << dev.minor << ") with board name " 
	   << dev.devicename << " and driver name " << dev.drivername << "." 
	   << endl;
      cout << "Found " 
	   << pluralize("acceptable subdevice", dev.subdevices.size()) 
	   << ":" << endl;
      
      for (uint j = 0; j < dev.subdevices.size(); j++) {
	ComediSubDevice & subdev = dev.subdevices[j];
	cout << "    Device " << subdev.id << " of type " << subdev.type
	     << " has " << pluralize("channel", subdev.n_channels) << " (" 
	     << (!subdev.used_by_rt_process ? "NOT " : "") 
	     << "used by rt_process, can " 
	     << (!subdev.can_be_locked ? "not " : "") << "be locked)" << endl;
	
      }
    }
  
    return (0);
  } catch (const Exception & e) {
    e.showMessage();
    return (-1);
  }
}

/* returns a string of the form: "qty s" with s properly pluralized */
QString
pluralize (const QString & s, int qty) 
{
  QString ret;
  
  ret.setNum(qty);
  ret += " " + s;

  if (qty != 1)
    ret += (ret[ret.length()-1] == 's' ? "es" : "s");

  return ret;
}

#endif

Probe::Probe(bool allowBusy, const QString & prefix) 
  : allowBusyDevices(allowBusy)
{

  QString devfile;
  const int n_devs_to_probe = 4;

  have_rt_process = false;

  /* probe to see if rt_process is loaded by checking for presence of shm */
  attach_to_shm_and_stuff();

  /* probe all possible /dev/comediXXX devices */
  for (int i = 0; i < n_devs_to_probe; i++) {    
    devfile = prefix + QString().setNum(i);
    try {
      do_probe(devfile);
    } catch (const NoComediDeviceException & e) {
      /* i guess we will let it slide... 
	 means no device is at /dev/comedi$i */
    }
  }

  /* throws a bunch of exceptions that can communicate to the outside world
     that there are no good devices to work with */
  validate();

  kill_shm(); /* release our shm since we only needed it temporarily */
}

/* Find a comedi device based on its device filename.  The returned
   device has property method isNull() == true if the device is not found */
const ComediDevice &
Probe::find(const QString & dev) const
{
  static const ComediDevice nullDevice;

  for (uint i = 0; i < probed_devices.size(); i++) {
    if (dev == probed_devices[i].filename) {
      return probed_devices[i];
    }
  }
  
  return nullDevice;
}


/* precondition: attempt to attach shm should have been made in caller */
void
Probe::do_probe(const QString & filename) 
{
  ComediDevice dev(filename);
  
  comedi_t *it = comedi_open(dev.filename.latin1());
  
  if (!it) {
    throw NoComediDeviceException(QString("No comedi devices found at ") 
				  + filename, 
				  "Please install Comedi and/or setup your "
				  "DAQ board correctly. Also, verify your "
				  "DAQ System program settings.");
  }

  cerr << "If you see any BUG error messages above, please ignore them!" << endl;

  /* try and determine the minor number */
  struct stat statbuf;
  fstat(comedi_fileno(it), &statbuf);
  dev.minor = minor(statbuf.st_rdev);
  dev.devicename = comedi_get_board_name(it);
  dev.drivername = comedi_get_driver_name(it); 

  const int n = 2;
  int types_to_probe[n] = {COMEDI_SUBD_AI, COMEDI_SUBD_AO};

  for (int i = 0; i < n; i++) {
    ComediSubDevice sdev;

    sdev.id = comedi_find_subdevice_by_type(it, types_to_probe[i], 0);
    if (sdev.id < 0) {
      continue;
    }
    sdev.type = ComediSubDevice::int2sd(types_to_probe[i]);
    
    sdev.n_channels = comedi_get_n_channels(it, sdev.id);
    if (sdev.n_channels) {
      sdev.ranges().resize(comedi_get_n_ranges(it, sdev.id, 0));
      for (uint j = 0; j < sdev.ranges().size(); j++) {
	sdev.ranges()[j] = *comedi_get_range(it, sdev.id, 0, j);
      }
      sdev.maxData() = comedi_get_maxdata(it, sdev.id, 0);
    }

    sdev.used_by_rt_process = 
      have_rt_process && (i == 0 ? shm->ai_minor : shm->ao_minor) == dev.minor;
    sdev.can_be_locked = comedi_lock(it, sdev.id) >= 0;
    comedi_unlock(it, sdev.id);

    if (allowBusyDevices || sdev.used_by_rt_process || sdev.can_be_locked) {
      dev.subdevices.insert(dev.subdevices.end(), sdev);
    }

  }

  if (!dev.subdevices.empty()) {
    probed_devices.insert(probed_devices.end(), dev);
  }
}

void
Probe::validate() const
{
  uint i;

  /* try and find at least 1 comedi analog input device */
  for (i = 0; i < probed_devices.size(); i++) {
    if (!probed_devices[i].find(ComediSubDevice::AnalogInput).isNull())
      return;
  }
  
  // we don't have at least 1 analog input device

  throw NoComediDeviceException("Missing AI subdevice", 
                                "Required analog input subdevice not found.  "
                                "You may have an incompatible board, or your "
                                "board may be configured incorrectly.\n\n"
                                "If you are using the rt_process.o driver, "
                                "there maybe have been a problem attaching  "
                                "to the real-time task due to permissions or "
                                "other assored setup issues.");
}

void
Probe::attach_to_shm_and_stuff()
{
  // temporary hack -- UGLY!
  struct stat statbuf;

  have_rt_process = !stat("/proc/rtlab", &statbuf);

  try {
    shm = ShmController::attach(ShmController::MBuff);
    misc = static_cast<int>(ShmController::MBuff);

  } catch (ShmException & e) {
    try { 
      shm = ShmController::attach(ShmController::RTAI_Shm);
      misc = static_cast<int>(ShmController::RTAI_Shm);
    } catch (ShmException & e) {
      if (have_rt_process) 
	throw ShmException("Error accessing shared memory device.", 
			   "Even though the RTLab kernel module is running, "
			   "the shared memory segment could not be attached.\n\n"
			   "This could be because the device node is somehow "
			   "inaccessible.\n\nMake sure that (if you are running "
			   "RTLinux) /dev/mbuff or (if you are running "
			   "RTAI) /dev/rtai_shm, exists and that this "
			   "device file is readable and writable by you.");
      
      have_rt_process = false;
      shm = 0;
    }
  }
}

void
Probe::kill_shm() /* Only to be called if instance initialization is done! */
{
  if (shm) 
    ShmController::detach(shm, static_cast<ShmController::ShmType>(misc));
}

/* static */
vector<ComediDevice> 
Probe::probeDevices()
{
  Probe p;

  return p.probed_devices;    
}
