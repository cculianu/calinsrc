/*
 * This file is part of the RT-Linux Multichannel Data Acquisition System
 *
 * Copyright (C) 1999,2000 David Christini
 * Copyright (C) 2001 David Christini, Lyuba Golub, Calin Culianu
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

#ifndef _LOG_H
#  define _LOG_H


#include <qmultilineedit.h>
#include <qstring.h>
#include <qfile.h>

#include "common.h"

class ExperimentLog: public QMultiLineEdit
{
 public:
  ExperimentLog (const QString & file, 
		 const QString & log_template = QString::null, 
		 QWidget * parent = 0, 
		 const char * name = 0);

  ~ExperimentLog();

  void setOutFile (const QString &f);
  
 private:
  QString readFile(const QString & fileName) const ;
  QString queryUserForFile(const QString & selected = QString(""),
			   bool forceUserToPickSomething = true) const;
  void openOutFile();


  QFile outFile; 
};

#endif
