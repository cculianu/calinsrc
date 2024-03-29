/*
 * Copyright (C) 1999, David Christini
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

#ifndef TEXTWINDOW_H
#define TEXTWINDOW_H

#include <qdialog.h>
#include <qlayout.h> 
#include <qmultilinedit.h> 
#include <qpushbutton.h>
#include <qtextbrowser.h>

//***** TextWindow
class TextWindow : public QWidget  //inherits QWidget class
{
  Q_OBJECT   //Q_OBJECT macro required for signals and slots
    public:
  TextWindow(QWidget *parent=0, const char *name=0, QString text="");
  ~TextWindow();
 private:
  QVBoxLayout *top_layout;      //overall window layout
  //QMultiLineEdit *display_text; //text to display ReadOnly
  QTextBrowser *display_text;
  QPushButton *kill_button;     //pushbutton to delete TextWindow
 public slots:
    void    KillWindow();
};

#endif
