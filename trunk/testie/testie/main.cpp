/***************************************************************************
                          main.cpp  -  description
                             -------------------
    begin                : Fri Aug  3 15:03:46 EDT 2001
    copyright            : (C) 2001 by Calin Culianu
    email                : calin@rtlab.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <qapplication.h>
#include <qfont.h>
#include <qstring.h>
#include <qtextcodec.h>
#include <qtranslator.h>

#include "testie.h"

int main(int argc, char *argv[])
{
  QApplication a(argc, argv);
  a.setFont(QFont("helvetica", 12));
  QTranslator tor( 0 );
  // set the location where your .qm files are in load() below as the last parameter instead of "."
  // for development, use "/" to use the english original as
  // .qm files are stored in the base project directory.
  tor.load( QString("testie.") + QTextCodec::locale(), "." );
  a.installTranslator( &tor );

  TestieApp *testie=new TestieApp();
  a.setMainWidget(testie);

  testie->show();

  if(argc>1)
    testie->openDocumentFile(argv[1]);
	else
	  testie->openDocumentFile();
	
  return a.exec();
}
