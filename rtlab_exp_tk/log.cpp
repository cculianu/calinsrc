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
#include <qcstring.h>
#include <qdatastream.h>

#include "log.h"
#include "exception.h"


ExperimentLog::
ExperimentLog(QWidget * p = 0, const char * n = 0)
  : QMultiLineEdit (p, n)
{
  init();
}

ExperimentLog::
ExperimentLog (const QString & f, const QString & t = QString::null, 
	       QWidget * p = 0, const char * n = 0)
  : QMultiLineEdit(p, n)

{
  init();
  useTemplate(t);
  setOutFile(f);
}

void
ExperimentLog::
init() /* initialization routine shared by constructors */
{
  setWordWrap(WidgetWidth);
  setWrapPolicy (AtWordBoundary);
  setTextFormat(Qt::PlainText);
}

ExperimentLog::
~ExperimentLog()
{
  if (_outFile.isOpen()) { saveOutFile(); _outFile.close(); }
}


const QString
ExperimentLog::
outFile() const 
{
  return _outFile.name();
}

/* closes current output file and attempts to open a new one.  If it doesn't
   exist this prompts the user to specify another file */
void
ExperimentLog::
setOutFile(const QString & f)
{
  bool ok = false;

  if (_outFile.isOpen()) _outFile.close();
  while (!ok) {
    _outFile.setName(f);
    try {
      openOutFile();
      ok = true;
    } catch (FileException & e) {
      e.showError();
      _outFile.setName(queryUserForFile(f));      
    }
  }
}

/* attempts to save the text() buffer to the output file -- may throw
   an application exception on error!! 
   ^^-- currently no error handling is implemented -Calin */
void
ExperimentLog::
saveOutFile()
{
  _outFile.reset(); /* just to make sure we are not appending but re-saving
		       everything */

  QDataStream s(&_outFile);
  s << text();
}

/* attempts to use log_template as the current template.  This implicitly
   replaces the text buffer with the contents of the file log_template.
   If log_template isn't valid, it just blanks out the buffer. */     
void
ExperimentLog::
useTemplate(const QString & log_template)
{
  setText(readFile(log_template));  
}


void
ExperimentLog::
openOutFile()
{
  if (! _outFile.open(IO_WriteOnly | IO_Truncate | IO_Translate)) {    
    QString msg(getQFileMessage(_outFile.status())), f(_outFile.name());
    throw FileException (QString("Error opening ") + f + " for writing.",
			 QString("Log output file ") + f + " could not be "
			 "opened for writing. " + msg);
  }
}

/* Reads a file at location "fileName" and cleans up the
   string a bit and then returns a QString containing its contents 
   returns a QString::null if there was a problem reading the file or
   it was empty */
QString
ExperimentLog::
readFile(const QString & fileName) 
{
  QFile f(fileName);
  QString ret = QString::null;

  if (f.exists() && f.open(IO_ReadOnly | IO_Translate)) {
    /* THIS IS BROKEN.  FOR SOME REASON YOU CAN'T RESIZE THE QBYTEARRAY! */
    QByteArray data(f.readAll());
    uint length; 
    if (!data.isNull() && (length = data.size()) > 0) {
      /* turn nulls to spaces just in case the file was like some strange 
	 binary/text mix */
      for (uint i = 0; i < length; i++) if (data[i] == 0) data[i] = 32;
      ret = QString(data);
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
		 bool forceUserToPickSomething = true)
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

const QString
ExperimentLog::
getQFileMessage(int status) 
{
  QString msg;
    switch (status) {
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
    return msg;
}
