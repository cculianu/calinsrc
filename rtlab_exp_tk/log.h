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
#include <qlayout.h>
#include <qlabel.h>

#include "common.h"

class ExperimentLog: public QWidget
{
  Q_OBJECT 

 public:
  /* constructs an ExperimentLog with no open outfiles.. use
     the property methods to set a template and outfile */
  ExperimentLog (QWidget * parent = 0, const char * name = 0);
  
  /* constructs an ExperimentLog with file as the output file and log_template
     as the log template file.  Attempts to open and/or read both files, and 
     may prompt the user to specify another file if it can't access
     one of the files passed in.  */
  ExperimentLog (const QString & file, 
                 const QString & log_template = QString::null, 
                 QWidget * parent = 0, 
                 const char * name = 0);

  ~ExperimentLog();
    
  /* returns the save file name -- null means not set */
  const QString & outFile() const { return _outFile; }

  bool unsavedStatus() const { return _unsaved_changes; }

  bool checkUnsaved(); /* checks if changes aren't saved and prompts to 
                          save if user elects to save 
                          returns false only if the user cancelled */

 public slots:
  /* Sets the save file.   */
  void setOutFile (const QString &f);
 
  /* attempts to use contents of file as the current text.  This implicitly
     replaces the text buffer with the contents of the file.
     If file isn't valid, it just blanks out the buffer. */     
  void loadTemplate (const QString & log_template);

  void load(); /* prompts user for a file, loads it, sets it to outFile */
  void save(); /* tries to save current file if its open, otherwise does
                  saveAs() */
  void saveAs(); /* prompts user for a file, then saves to it */
  void revert(); /* reverts the current text if there is a current outfile */

  void insertAtCursor(const QString & text);

 protected slots:

  void setUnsavedStatus(bool status) 
   { 
     if ((status && !_unsaved_changes) || (!status && _unsaved_changes)) 
       changedLabel.setEnabled(status);    
     _unsaved_changes = status; 
   }

 private slots:

  void changed() { setUnsavedStatus(true); }

 public:

 QMultiLineEdit & embeddedMultiLineEditorWidget() { return mle; }
 const QMultiLineEdit & embeddedMultiLineEditorWidget() const { return mle; }

 private:
  void init();
  

  /* attempts to save the text() buffer to the output file -- may throw
     an application exception on error!! */
  void saveOutFile ();

  static QString readFile(const QString & fileName);
  static const QString getQFileMessage(int status);

  QString _outFile; 
  bool _unsaved_changes;

  QGridLayout layout; /* this manages this widget's layout */
  QMultiLineEdit mle; /* the actual editable text */
  QLabel fileNameLabel, changedLabel;
};

#endif
