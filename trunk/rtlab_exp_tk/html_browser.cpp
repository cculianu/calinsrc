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
#include <qtextbrowser.h>
#include <qstring.h>
#include <qpushbutton.h>
#include <qpixmap.h>
#include <qdialog.h>
#include <qlayout.h>
#include <qhbox.h>
#include <qtooltip.h>

//#include "back.xpm"
#include "html_browser.h"

HTMLBrowser::HTMLBrowser( QWidget *parent = 0 ) 
  : QDialog( parent )
{
  mainBox = new QVBoxLayout( this );
  mainBox->setAutoAdd(true);
  navBox = new QHBox ( this ); 

  backButton = new QPushButton( "Back", navBox );
  QToolTip::add(backButton, "Go to previous document");
  //QPixmap back_pm(back_xpm);
  //backButton->setPixmap( back_pm );
  backButton->setMaximumSize( back_pm.size() );

  forwardButton = new QPushButton( "Forward", navBox );  
  homeButton = new QPushButton( "Home", navBox );
  helpTextBrowser = new QTextBrowser( this );
  mainBox->setStretchFactor(helpTextBrowser, 1);

  /* 
     Todo: Ask Lyuba if playing with stretch factors helps here?
  */

  forwardButton->resize( forwardButton->sizeHint() );
  homeButton->resize( homeButton->sizeHint() );

  resize(400, 400);

  connect( backButton, SIGNAL( clicked() ), 
           helpTextBrowser, SLOT( backward() ) );
  connect( forwardButton, SIGNAL( clicked() ), 
           helpTextBrowser, SLOT( forward() ) );
  connect( homeButton, SIGNAL( clicked() ), 
           helpTextBrowser, SLOT( home() ) );

  
}

void HTMLBrowser::openPage( QString page )
{
  helpTextBrowser->setSource( page );
  show();
  setActiveWindow();
  raise();
}

