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

#include <qfiledialog.h>

#include "log.h"
#include "exception.h"

ExperimentLog::
ExperimentLog (const QString & f, const QString & t = QString::null, 
	       QWidget * p = 0, const char * n = 0)
  : QMultiLineEdit(p, n)

{

  setWordWrap(WidgetWidth);
  setWrapPolicy (AtWordBoundary);
  setTextFormat(Qt::PlainText);
  setText(readFile(t));
  setOutFile(f);

}

ExperimentLog::
~ExperimentLog()
{
  if (outFile.isOpen()) outFile.close();
}


void
ExperimentLog::
setOutFile(const QString & f)
{
  bool ok = false;

  if (outFile.isOpen()) outFile.close();
  while (!ok) {
    outFile.setName(f);
    try {
      openOutFile();
      ok = true;
    } catch (FileException & e) {
      e.showError();
      outFile.setName(queryUserForFile(f));      
    }
  }
}

/* Reads a file at location "fileName" and cleans up the
   string a bit and then returns a QString containing its contents 
   returns a QString::null if there was a problem reading the file or
   it was empty */
QString
ExperimentLog::
readFile(const QString & fileName) const
{
  QFile f(fileName);
  QString ret = QString::null;

  if (f.exists() && f.open(IO_ReadOnly | IO_Translate)) {
    QIODevice::Offset buflen = f.size();
    if (buflen > 0) {
      char *buf = new char[buflen+1]; // ok, we used the heap here to be safe..
      Q_LONG n_bytes = f.readBlock(buf, sizeof(buf));
      if (n_bytes > 0) {
	/* turn nulls to spaces just in case the file was like some strange 
	   binary/text mix */
	for (Q_LONG i = 0; i < n_bytes; i++) if (buf[i] == 0) buf[i] = 32;
	buf[n_bytes] = 0; /* add trailing null as readBlock() doesn't */
	ret = QString(buf);
      }
      delete buf;
    }
  }
  return ret;
}

/* returns QString::null if forceUserToPickSomething is false and the user
   cancelled the request, otherwise returns the filename the user
   specified */
QString
ExperimentLog::
queryUserForFile(const QString & selected, 
		 bool forceUserToPickSomething = true) const
{
  static const char *filters[2] = {"All files (*)", 0};
  QFileDialog fileDialog(0, 0, true);

  fileDialog.setMode(QFileDialog::AnyFile);
  fileDialog.setViewMode(QFileDialog::Detail);
  fileDialog.setFilters(filters);

  fileDialog.setSelection(selected);
    
  /* keep retrying until they pick a damned file */
  while (fileDialog.exec() != QDialog::Accepted && forceUserToPickSomething) 
    fileDialog.setSelection(fileDialog.selectedFile());    
  

  return (QDialog::Accepted ? fileDialog.selectedFile() : QString::null);    
}

void
ExperimentLog::
openOutFile()
{
  if (! outFile.open(IO_WriteOnly | IO_Truncate | IO_Translate)) {    
    QString msg, f(outFile.name());
    switch (outFile.status()) {
    case IO_ReadError:
      msg = "Could not read from the device.";
      break;
    case IO_WriteError:
      msg = "Could not write to the device.";
      break;
    case IO_FatalError: 
      msg = "A fatal unrecoverable error occurred.";
      break;
    case IO_OpenError:
      msg = "Could not open the device.";
      break;
    case IO_AbortError: 
      msg = "The operation was unexpectedly aborted.";
      break;
    case IO_TimeOutError: 
      msg = "The operation timed out.";
      break;
    case IO_UnspecifiedError:
      msg = "An unspecified error happened on close.";
      break;
    case IO_Ok:
    default:
      msg = "The operation was successful.";
      break;
    }
    throw FileException (QString("Error opening ") + f + " for writing.",
			 QString("Log output file ") + f + " could not be "
			 "opened for writing. " + msg);
  }
}
