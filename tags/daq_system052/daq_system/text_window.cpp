/*
 * This file is part of the "multiple_ecg" software  package.
 *
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
#include <qstylesheet.h>
#include "text_window.h"

TextWindow::TextWindow(QWidget *parent, const char *name, QString text)
    : QWidget(parent, name, 0)  //inherits QWidget class
{
  top_layout = new QVBoxLayout(this, 10);
  display_text = new QTextBrowser(this);

  display_text->setStyleSheet(QStyleSheet::defaultSheet());
  display_text->setText(text);
  top_layout->addWidget(display_text,9);

  kill_button = new QPushButton("OK", this);
  kill_button->setMaximumSize(60,30);
  kill_button->setFont(QFont("Helvetica",15,QFont::Bold));
  kill_button->setDefault(TRUE);
  top_layout->addWidget(kill_button,1,AlignCenter);

  top_layout->activate();
  connect(kill_button, SIGNAL(clicked()), this, SLOT(KillWindow()));
}

TextWindow::~TextWindow(){
  delete top_layout;
  delete display_text;
  delete kill_button;
};

void TextWindow::KillWindow() {
  delete this;
}
