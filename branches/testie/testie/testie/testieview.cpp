/***************************************************************************
                          testieview.cpp  -  description
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

// include files for Qt
#include <qprinter.h>
#include <qpainter.h>

// application specific includes
#include "testieview.h"
#include "testiedoc.h"


TestieView::TestieView(TestieDoc* pDoc, QWidget *parent, const char* name, int wflags)
 : QWidget(parent, name, wflags)
{
    doc=pDoc;
}

TestieView::~TestieView()
{
}

TestieDoc *TestieView::getDocument() const
{
	return doc;
}

void TestieView::update(TestieView* pSender){
	if(pSender != this)
		repaint();
}

void TestieView::print(QPrinter *pPrinter)
{
  if (pPrinter->setup(this))
  {
		QPainter p;
		p.begin(pPrinter);
		
		///////////////////////////////
		// TODO: add your printing code here
		///////////////////////////////
		
		p.end();
  }
}

void TestieView::closeEvent(QCloseEvent*)
{
  // LEAVE THIS EMPTY: THE EVENT FILTER IN THE TestieApp CLASS TAKES CARE FOR CLOSING
  // QWidget closeEvent must be prevented.
}

