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

#ifndef _HELP_BROWSER_H
#define _HELP_BROWSER_H

#include <qstring.h>
#include <qdialog.h>

class QVBoxLayout; class QHBox; class QPushButton; class QTextBrowser;


class HTMLBrowser : public QDialog
{
  Q_OBJECT
 public:

  HTMLBrowser ( QWidget *parent = 0);

  void openPage ( QString page );

 protected:
  QTextBrowser *helpTextBrowser;

 private:

  QVBoxLayout *mainBox;
  QHBox *navBox;
  QPushButton *backButton, *forwardButton, *homeButton;
  
};
#endif
