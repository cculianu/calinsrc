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

#include <qfile.h>
#include <qfiledialog.h>
#include <qmessagebox.h>
#include <qcstring.h>
#include <qtextstream.h>

#include "simple_text_editor.h"
#include "exception.h"

#define LOG_CLASS_CONSTRUCTOR_INIT_LIST \
  QWidget(p, n), layout (this, 3, 2), menuBar(this), mle(this), \
  fileNameLabel(QString::null, this), changedLabel("MOD", this)


SimpleTextEditor::
SimpleTextEditor(QWidget * p = 0, const char * n = 0) 
  : LOG_CLASS_CONSTRUCTOR_INIT_LIST
{
  init();
  setOutFile(QString::null);
}

SimpleTextEditor::
SimpleTextEditor (const QString & f, const QString & t = QString::null, 
               QWidget * p = 0, const char * n = 0)
  : LOG_CLASS_CONSTRUCTOR_INIT_LIST
{
  init();
  loadTemplate(t);
  setOutFile(f);
}

#undef LOG_CLASS_CONSTRUCTOR_INIT_LIST

void
SimpleTextEditor::
init() /* initialization routine shared by constructors */
{
  setUnsavedStatus(false);
  mle.setUndoRedoEnabled(true);
  mle.setUndoDepth(100);
  mle.setWordWrap(QMultiLineEdit::WidgetWidth);
  mle.setWrapPolicy (QMultiLineEdit::AtWordBoundary);
  mle.setTextFormat(Qt::PlainText);
  mle.setVScrollBarMode(QScrollView::Auto);
  mle.setHScrollBarMode(QScrollView::Auto);
  connect (&mle, SIGNAL(textChanged()), this, SLOT(changed()));

  QFont labelFont;
  labelFont.setStyleHint(QFont::Helvetica);
  labelFont.setPointSize(8);

  fileNameLabel.setFont(labelFont);
  changedLabel.setFont(labelFont);

  layout.setRowStretch(1,1);
  layout.setColStretch(0,1);
  layout.addMultiCellWidget(&menuBar, 0, 0, 0, 1);
  layout.addMultiCellWidget(&mle, 1, 1, 0, 1);
  layout.addWidget(&fileNameLabel, 2, 0);
  layout.addWidget(&changedLabel, 2, 1);

  
  fileMenu.insertItem("&Load...", this, SLOT( load() ));
  saveMenuItemId = 
    fileMenu.insertItem("&Save", this, SLOT( save() ), CTRL + Key_S);
  fileMenu.insertItem("Save &As...", this, SLOT( saveAs() ));
  revertMenuItemId = 
    fileMenu.insertItem("&Revert", this, SLOT ( revert() ));

  undoMenuItemId =
    editMenu.insertItem("&Undo", &mle, SLOT ( undo() ), CTRL + Key_Z);
  redoMenuItemId =
    editMenu.insertItem("&Redo", &mle, SLOT ( redo() ), CTRL + SHIFT + Key_Z);
  editMenu.insertSeparator();
  cutMenuItemId =
    editMenu.insertItem("Cu&t", &mle, SLOT(cut()), CTRL + Key_X);
  copyMenuItemId =
    editMenu.insertItem("&Copy", &mle, SLOT(copy()), CTRL + Key_C);
  editMenu.insertItem("&Paste", &mle, SLOT(paste()), CTRL + Key_V);
  editMenu.insertItem("C&lear All", &mle, SLOT ( clear() ), CTRL + Key_U);

  menuBar.insertItem("&File", &fileMenu);
  menuBar.insertItem("&Edit", &editMenu);

  copyAvail(false); undoAvail(false), redoAvail(false);
  connect(&mle, SIGNAL(copyAvailable(bool)), this, SLOT(copyAvail(bool)));
  connect(&mle, SIGNAL(undoAvailable(bool)), this, SLOT(undoAvail(bool)));
  connect(&mle, SIGNAL(redoAvailable(bool)), this, SLOT(redoAvail(bool)));

}

SimpleTextEditor::
~SimpleTextEditor()
{
  /* nothing */
}

/* Sets the save file.   */
void 
SimpleTextEditor::
setOutFile (const QString &f)
{ 
  _outFile = f; 
  if (_outFile.isNull() ) {
    fileNameLabel.setText("(untitled)");
    fileMenu.setItemEnabled(saveMenuItemId, false);
  } else {
    fileNameLabel.setText(_outFile.section('/',-1));
    fileMenu.setItemEnabled(saveMenuItemId, true);
  }
  setCaption(QString(name()) + " - " + fileNameLabel.text());
}

/* attempts to use contents of file as the current text.  This implicitly
   replaces the text buffer with the contents of the file.
   If file isn't valid, it just blanks out the buffer. */     
void
SimpleTextEditor::
loadTemplate(const QString & file)
{
  if (!checkUnsaved()) /* user cancelled */ return;
  mle.setText(readFile(file));  
  setUnsavedStatus(false);
}


void
SimpleTextEditor::
load()
{ 
  if (!checkUnsaved()) /* user cancelled */ return;

  QString f( QFileDialog::getOpenFileName( outFile(),
                                           "Logs (*.log *.txt)", this ) );
  if ( f.isEmpty() )
    return;

  setUnsavedStatus(false); /* hack, but it works -- needed to prevent the
                              other checkUnsaved() call in loadTemplate() */

  loadTemplate( f ); 
  setOutFile(f);
}

void
SimpleTextEditor::
save()
{

  if (outFile().isNull()) {
    saveAs();
    return;
  } 
  try {    
    saveOutFile();
  } catch (FileException & e) {
    e.showError();
    saveAs();
  }
}

void 
SimpleTextEditor::
saveAs()
{
  QString f( QFileDialog::getSaveFileName( outFile(), 
                                           "Logs (*.log *.txt)", this ));
  if (f.isNull()) return;

  QFile file (f);
  if (file.exists()) {
    int buttonPicked =
      QMessageBox::
      warning(this, 
              "File exists.", 
              QString("The file ") + f + " exists.\nIf you choose to continue,"
              " it will be overwritten.",
              "Overwrite", "Choose Another", "Cancel", 0, 2);    
    switch (buttonPicked) {      
    case 1:
      saveAs();
    case 2:
      return;     
    case 0:
      break;
    }
  }
  setOutFile(f);
  save();
}

void
SimpleTextEditor::
revert()
{
  if (!outFile().isNull()) {
    loadTemplate(outFile());
  }
}

void
SimpleTextEditor::insertAtCursor (const QString & text) 
{
  int row, col;
  mle.getCursorPosition(&row, &col);
  mle.insertAt(text, row, col);
  mle.setCursorPosition(row, col + text.length());
}

bool
SimpleTextEditor::
checkUnsaved() 
{  
  if (unsavedStatus()) {
    int userSaid =
    QMessageBox::
      warning(this, 
              "The log is unsaved.", 
              "The log is unsaved.  Do you wish to save changes, discard "
              "changes, or cancel?",
              "Save Changes", "Discard", "Cancel", 0, 2);
    switch (userSaid) {
    case 0: /* save changes */
      save();
      QMessageBox::information(this, "Changes saved.", 
                               QString("Changes saved to '%1'.").arg(outFile()), 
                               QMessageBox::Ok);
      break;
    case 1: /* discard changes */
      break;
    case 2: /* cancel */
      return false;
      break;
    }
  }
  return true;

}

/* attempts to save the text() buffer to the output file -- may throw
   an application exception on error!! */
void
SimpleTextEditor::
saveOutFile()
{
  QFile out(outFile());

  if (! out.open(IO_WriteOnly | IO_Translate | IO_Truncate)) {    
    QString msg(getQFileMessage(out.status())), f(out.name());
    throw FileException (QString("Error opening ") + f + " for writing.",
                         QString("Log output file ") + f + " could not be "
                         "opened for writing. " + msg);
  }

  QTextStream s(&out);

  s.setEncoding(QTextStream::Latin1);
  s << mle.text();
  out.flush();
  setUnsavedStatus(false);
}

/* Reads a file at location "fileName" and cleans up the
   string a bit and then returns a QString containing its contents 
   returns a QString::null if there was a problem reading the file or
   it was empty */
QString
SimpleTextEditor::
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
    f.close();
  }
  return ret;
}

const QString
SimpleTextEditor::
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
