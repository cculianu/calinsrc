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
extern "C" {
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
}
#include <iostream>
#include <qapplication.h>
#include <qnamespace.h>
#include <qmainwindow.h>
#include <qworkspace.h>
#include <qstring.h>
#include <qmessagebox.h>
#include "common.h"
#include "exception.h"
#include "probe.h"
#include "daq_settings.h"
#include "configuration.h"
#include "daq_system.h"

static void testAndMakeDaqSystemDirOrQuit(void);
static void init(void);
static void uninit();

Probe *probe = 0;
DAQSettings *settings = 0;
ConfigurationWindow *conf = 0;
DAQSystem *daqSystem = 0;

int 
main(int argc, char *argv[])
{
  QApplication a(argc, argv);
  bool showConfigScreen;
  int retval = 0;

 try_again:
  try {

    init(); /* this may throw an exception on error */
    showConfigScreen = settings->getShowConfigOnStartup();

    /* display configuration window, if we're supposed to */
    do {
      conf->startupScreenSemantics = true;
      if ( showConfigScreen && !conf->exec() ) {
        uninit();
        return (1);
      }
    } while (!DAQSystem::isValidDAQSettings(*settings) 
             && (showConfigScreen = true) );

    daqSystem = new DAQSystem(*conf, 0, DAQ_SYSTEM_APPNAME_CSTRING, 
                              Qt::WType_TopLevel | Qt::WDestructiveClose );    
    a.setMainWidget(daqSystem);     
    
    daqSystem->show();

    retval = a.exec();
    
  } catch (const UnimplementedException & e) {
    e.showError();
    showConfigScreen = true;
    uninit();
    goto try_again;
  } catch (const Exception & e) {
    e.showError();
    if (settings) {
      settings->setShowConfigOnStartup(true); // force this for next time 
      settings->saveSettings();
    }
    retval = 1;
  }
  uninit();
  
  return retval;
}

static
void
init(void)
{
    testAndMakeDaqSystemDirOrQuit(); // exits on error  
    probe = new Probe();
    settings = new DAQSettings;
    conf = 
      new ConfigurationWindow (*probe, *settings, 0, "Configuration Window", 
                               Qt::WType_TopLevel | Qt::WType_Modal);
}

static
void
uninit(void)
{
  if (conf) delete conf; conf = 0;
  if (settings) delete settings; settings = 0;
  if (probe) delete probe; probe = 0;
  /* do not delete because of WDestructiveClose ? 
     if (daqSystem) delete daqSystem; daqSystem = 0; */  
}

static
void
testAndMakeDaqSystemDirOrQuit(void)
{
  /* todo: move this dir checking business into something like
     the settings class or somesuch */
  struct stat statbuf;
  bool created_dir = false;

  /* I apologize for this ugly if condition... -calin */
  if ((stat(DAQ_SYSTEM_USER_DIR, &statbuf) < 0 || !S_ISDIR(statbuf.st_mode))
      && !(created_dir = (mkdir(DAQ_SYSTEM_USER_DIR, 0755) >= 0)) ) {
    int saved_errno = errno;
    throw Exception ("Error creating daq system user directory", 
		     QString("Cannot create daq system user directory: ") 
		     + DAQ_SYSTEM_USER_DIR + "\nError was: " 
		     + sys_errlist[saved_errno]);
    errno = saved_errno;
    exit(saved_errno);
  } else if (created_dir) {
    cerr << "Daq System Non-Critical Information: Created directory " 
	 << DAQ_SYSTEM_USER_DIR << endl;
  }
}

