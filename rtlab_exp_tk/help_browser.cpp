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
#include <iostream.h>
#include <string>
#include <qtextbrowser.h>
#include <qstring.h>
#include <qpushbutton.h>
#include <qdialog.h>
#include <qvbox.h>

#include "daq_help_sources.h"
#include "help_browser.h"

HelpBrowser * HelpBrowser::defaultBrowser = 0;

HelpBrowser::HelpBrowser( QWidget *parent = 0 ) :
  QDialog( parent ),
  mainBox( this ),
  navBox( &mainBox ),
  backButton( "Back", &navBox ),
  forwardButton( "Forward", &navBox ),
  homeButton( "Home", &navBox ),
  helpTextBrowser( &mainBox )
{
  helpTextBrowser.setMimeSourceFactory( DAQHelpSources::initFactory() );
  helpTextBrowser.setSource( DAQHelpSources::mainWindowHelpSource );
  backButton.resize( backButton.sizeHint() );
  forwardButton.resize( forwardButton.sizeHint() );
  homeButton.resize( homeButton.sizeHint() );
  navBox.resize(400,400);
  mainBox.resize(400, 400);
  this->resize(400, 400);

  connect( &backButton, SIGNAL( clicked() ), &helpTextBrowser, SLOT( backward() ) );
  connect( &forwardButton, SIGNAL( clicked() ), &helpTextBrowser, SLOT( forward() ) );
  connect( &homeButton, SIGNAL( clicked() ), &helpTextBrowser, SLOT( home() ) );
}

void HelpBrowser::openPage( QString page )
{
  helpTextBrowser.setSource( page );
  this->show();
  this->setActiveWindow();
  this->raise();
}

HelpBrowser * HelpBrowser::getDefaultBrowser()
{
  if ( defaultBrowser == 0 )
    defaultBrowser = new HelpBrowser();    
  return defaultBrowser;

}
