/***************************************************************************
                          testiedoc.cpp  -  description
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
#include <qdir.h>
#include <qfileinfo.h>
#include <qwidget.h>
#include <qmsgbox.h>
#include <qfiledialog.h>


// application specific includes
#include "testiedoc.h"
#include "testie.h"
#include "testieview.h"


TestieDoc::TestieDoc()
{
  pViewList = new QList<TestieView>;
  pViewList->setAutoDelete(false);
}

TestieDoc::~TestieDoc()
{
  delete pViewList;
}

void TestieDoc::addView(TestieView *view)
{
  pViewList->append(view);
	changedViewList();
}

void TestieDoc::removeView(TestieView *view)
{
	  pViewList->remove(view);
	  if(!pViewList->isEmpty())
			changedViewList();
		else
			deleteContents();
}

void TestieDoc::changedViewList(){	
	
	TestieView *w;
	if((int)pViewList->count() == 1){
  	w=pViewList->first();
  	w->setCaption(m_title);
	}
	else{
		int i;
    for( i=1,w=pViewList->first(); w!=0; i++, w=pViewList->next())
  		w->setCaption(QString(m_title+":%1").arg(i));	
	}
}

bool TestieDoc::isLastView() {
  return ((int) pViewList->count() == 1);
}


void TestieDoc::updateAllViews(TestieView *sender)
{
  TestieView *w;
  for(w=pViewList->first(); w!=0; w=pViewList->next())
  {
     w->update(sender);
  }

}

void TestieDoc::setPathName(const QString &name)
{
  m_filename=name;
	m_title=QFileInfo(name).fileName();
}

const QString& TestieDoc::pathName() const
{
  return m_filename;
}

void TestieDoc::setTitle(const QString &title)
{
  m_title=title;
}

const QString &TestieDoc::title() const
{
  return m_title;
}


void TestieDoc::closeDocument()
{
  TestieView *w;
  if(!isLastView())
  {
    for(w=pViewList->first(); w!=0; w=pViewList->next())
    {
   	 	if(!w->close())
 				break;
    }
	}
  if(isLastView())
  {
  	w=pViewList->first();
  	w->close();
  }
}

bool TestieDoc::newDocument()
{
  /////////////////////////////////////////////////
  // TODO: Add your document initialization code here
  /////////////////////////////////////////////////
  modified=false;
  return true;
}

bool TestieDoc::openDocument(const QString &filename, const char *format /*=0*/)
{

	QFile f( filename );
	if ( !f.open( IO_ReadOnly ) )
		return false;
  /////////////////////////////////////////////////
  // TODO: Add your document opening code here
  /////////////////////////////////////////////////
	f.close();
	
  modified=false;
  m_filename=filename;
	m_title=QFileInfo(f).fileName();
  return true;
}

bool TestieDoc::saveDocument(const QString &filename, const char *format /*=0*/)
{
	QFile f( filename );
	if ( !f.open( IO_WriteOnly ) )
		return false;

  /////////////////////////////////////////////////
  // TODO: Add your document saving code here
  /////////////////////////////////////////////////

  f.close();

  modified=false;
  m_filename=filename;
	m_title=QFileInfo(f).fileName();
  return true;
}

void TestieDoc::deleteContents()
{
  /////////////////////////////////////////////////
  // TODO: Add implementation to delete the document contents
  /////////////////////////////////////////////////

}

bool TestieDoc::canCloseFrame(TestieView* pFrame)
{
	if(!isLastView())
		return true;
		
	bool ret=false;
  if(isModified())
  {
		QString saveName;
  	switch(QMessageBox::information(pFrame, title(), tr("The current file has been modified.\n"
                          "Do you want to save it?"),QMessageBox::Yes, QMessageBox::No, QMessageBox::Cancel ))
    {
			case QMessageBox::Yes:
				if(title().contains(tr("Untitled")))
				{
					saveName=QFileDialog::getSaveFileName(0, 0, pFrame);
                                          if(saveName.isEmpty())
          	                                return false;
				}
				else
					saveName=pathName();

				if(!saveDocument(saveName))
				{
 					switch(QMessageBox::critical(pFrame, tr("I/O Error !"), tr("Could not save the current document !\n"
																												"Close anyway ?"),QMessageBox::Yes ,QMessageBox::No))

 					{
 						case QMessageBox::Yes:
 							ret=true;
 						case QMessageBox::No:
 							ret=false;
 					}
				}
				else
					ret=true;
				break;
			case QMessageBox::No:
				ret=true;
				break;
			case QMessageBox::Cancel:
			default:
				ret=false;
				break;
		}
	}
	else
		ret=true;

	return ret;
}
