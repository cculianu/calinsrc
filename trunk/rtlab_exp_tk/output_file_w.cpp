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

#include "output_file_w.h"
#include <qlayout.h>
#include <qdialog.h>
#include <qfiledialog.h>
#include <qlineedit.h>
#include <qgroupbox.h>
#include <qhbox.h>
#include <qhbuttongroup.h>
#include <qradiobutton.h>
#include <qpushbutton.h>
#include <qcheckbox.h>
#include <qaccel.h>
#include <qmessagebox.h> 
#include <qlabel.h>


OutputFileW::OutputFileW(DAQSettings *s, QWidget * parent, const char * name, WFlags f)
  : QWidget(parent, name, f), settings(s)
{
  outputFileSelectionGrid = new QGridLayout(this, 3, 2);

  
  outputFileRadioGroup = new QHButtonGroup(0); /* Warning! No parent here! */
  QLabel *outputFileLabel = new QLabel("Output file format:", this);
  QHBox *radioButtonHBox = new QHBox(this);
  asciiRadio  = new QRadioButton(radioButtonHBox);
  binaryRadio = new QRadioButton(radioButtonHBox);
  outputFileRadioGroup->insert(asciiRadio, 0);
  outputFileRadioGroup->insert(binaryRadio, 1);
  outputFile = new QLineEdit(this);
  browseOutputFiles = new QPushButton("Browse...", this);
  
  outputFileSelectionGrid->addWidget(outputFile, 0, 0);
  outputFileSelectionGrid->addWidget(browseOutputFiles, 0, 1);
  outputFileSelectionGrid->addWidget(outputFileLabel, 1, 0);
  outputFileSelectionGrid->addMultiCellWidget(radioButtonHBox, 2, 2, 0, 1); 
  asciiRadio->setText("Output Ascii");
  binaryRadio->setText("Output Binary");
  connect(browseOutputFiles, SIGNAL(clicked(void)),
          this, SLOT(askUserForOutputFilename(void)));
  connect(asciiRadio, SIGNAL(clicked()),
          this, SLOT(resuffixIze()));
  connect(binaryRadio, SIGNAL(clicked()),
          this, SLOT(resuffixIze()));

}

OutputFileW::~OutputFileW()
{
  delete outputFileRadioGroup; outputFileRadioGroup = 0;  
}

QString OutputFileW::fileName() const { return outputFile->text().stripWhiteSpace(); }

DAQSettings::DataFileFormat OutputFileW::fileFormat() const 
{ 
  return (asciiRadio->isChecked() 
          ?  DAQSettings::Ascii : DAQSettings::Binary); 
}

void OutputFileW::setFileName(const QString & fn) 
{ outputFile->setText(fn.stripWhiteSpace()); }

void OutputFileW::setFileFormat(DAQSettings::DataFileFormat fmt) 
{  
  fmt == DAQSettings::Ascii 
    ? asciiRadio->setChecked(true) 
    : binaryRadio->setChecked(true);
}


void
OutputFileW::askUserForOutputFilename()
{
  QString filter = 
    QString ("DAQ System Data Files (*%1 *%2)")
    .arg(DAQSettings::fileFormatExts[DAQSettings::Binary])
    .arg(DAQSettings::fileFormatExts[DAQSettings::Ascii]);

  QFileDialog fileDialog(this, 0, true);

  fileDialog.setMode(QFileDialog::AnyFile);
  fileDialog.setViewMode(QFileDialog::Detail);
  fileDialog.setFilter(filter);

  fileDialog.setSelection(outputFile->text());

  if (fileDialog.exec()) {
    outputFile->setText(fileDialog.selectedFile());
    resuffixIze();
  }
}

void OutputFileW::resuffixIze()
{
  QString desired_suffix ( DAQSettings::fileFormatExts[fileFormat()] );
  
  outputFile->setText(forceFilenameExt(outputFile->text(), desired_suffix));
}

void OutputFileW::toSettings()
{
  if (settings) {
    resuffixIze(); /* as per Dave's request.. everything is forced the right
                      suffix! */
    settings->setDataFileFormat(fileFormat());
    settings->setDataFile(fileName());    
  }
}

void OutputFileW::fromSettings()
{
  if (settings) {
    setFileFormat(settings->getDataFileFormat());
    setFileName(settings->getDataFile());
  }
}

void OutputFileW::showEvent (QShowEvent * e)
{
  e = e; /* keep compiler happy */
  fromSettings();
}
