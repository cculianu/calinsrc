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
#include <qapplication.h>
#include <qstring.h>
#include <qmessagebox.h>
#include <iostream>
#include "common.h"
#include "exception.h"


Exception::Exception (const QString & briefMsg, const QString & fullMsg,
                      ErrorReportingMode m)
  : _briefMsg(briefMsg), _fullMsg(fullMsg), _errorReportingMode(m) {}

void 
Exception::showError() const
{
  if (qApp != NULL) { 
    __showError();
  } else {
    /* try once to create some Qt resources so __showError() can display
       a critical message box */
    int argc = 1;
    char *args[] = {"none", 0};
    QApplication a(argc, args);
    __showError();
  }
}

void 
Exception::showConsoleError() const
{

  int llen = (78 - (static_cast<int>(briefMsg().length())+2) ) / 2, i;
  bool add1 = llen * 2 != (78 - (static_cast<int>(briefMsg().length())+2) );

  if (llen <= 0) llen = 0;

  QString leftDashes = "", rightDashes = "";

  for (i = 0; i < llen; i++) leftDashes += "-";
  rightDashes = leftDashes + ( add1 ? "-" : "" );
  
  /* first show the error on the console... */
  std::cerr 
    << leftDashes.latin1() << " " << briefMsg().latin1() << " " 
    << rightDashes.latin1() << std::endl
    << briefMsg().latin1() << std::endl 
    << std::endl 
    << fullMsg().latin1() << std::endl
    << "------------------------------------------------"
    << "------------------------------" 
    << std::endl;

}

void
Exception::showGUIError() const
{
  /* now, if possible, show it graphically... */
  if (qApp != NULL && errorReportingMode() == GUI )
    QMessageBox::critical ( 0, briefMsg(), fullMsg() );
}

void 
Exception::__showError() const
{
  showConsoleError();
  showGUIError();
}

UnimplementedException::UnimplementedException 
(
 const QString & brief,
 const QString & full,
 ErrorReportingMode m
) 
  : Exception(brief, full, m) {}


NoComediDeviceException::NoComediDeviceException 
(
 const QString & brief,
 const QString & full,
 ErrorReportingMode m
) 
  : Exception(brief, full, m) {}


ShmException::ShmException 
(
 const QString & brief,
 const QString & full,
 ErrorReportingMode m
) 
  : Exception  (brief, full, m) {}

RTPException::RTPException 
(
 const QString & brief,
 const QString & full,
 ErrorReportingMode m
) 
  : Exception  (brief, full, m) {}


MBuffException::MBuffException
(
 const QString & mbuff_file,
 ErrorReportingMode m
) 
  : Exception  (QString ("Error occurred with %1").arg(mbuff_file), 
		QString ("An operation upon/with the device file %1 failed! "
			 "Common sources of such failures are permissions "
			 "issues (make sure %2 is chmod 666), or a non-existant "
			 "or invalid %3."
			).arg(mbuff_file).arg(mbuff_file).arg(mbuff_file), 
		m) {}
