/*
 * This file is part of the RT-Linux Multichannel Data Acquisition System
 *
 * Copyright (C) 1999,2000 David Christini
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
#ifndef CONFIGURATION_H
#define CONFIGURATION_H


#include <iostream>   
#include <vector>
#include <fstream.h>    
#include <stdlib.h> //for random()    


#include <qstring.h>
#include <qdialog.h>
#include <qfiledialog.h>
#include <qurl.h>
#include <qlayout.h>
#include <qgroupbox.h> 
#include <qhbox.h>
#include <qlabel.h> 
#include <qmultilinedit.h> 
#include <qpushbutton.h>
#include <qradiobutton.h>
#include <qcheckbox.h>
#include <qbuttongroup.h>
#include <qlistview.h>
#include <qtextview.h>
#include <qlineedit.h>
#include "daq_settings.h"
#include "probe.h"
#include "comedi_device.h"


/* 
   ToDo: 
   1) Figure out a better way to merge the settings model-type class
   with the ConfigurationWindow 'view'.  Maybe via slots?

   2) Figure out a better way of making the Settings class more maintainable
   and have fewer lines of code.
*/
class ConfigurationWindow : public QDialog 
{
  Q_OBJECT  
 public:
  ConfigurationWindow(const Probe & deviceProbe, DAQSettings & settings,
                      QWidget *parent = 0, const char *name = 0, WFlags f = 0);


  DAQSettings & daqSettings() const { return settings; }

  void show(); /* just a thin wrapper for QDialog::show() that 
		  calls fromSettings() first */

  /* a thin wrapper to the DeviceListView */
  const ComediDevice & selectedDevice() const 
    { return deviceTable.selectedDevice(); }

  /* this controls the behavior of what is considered valid input
     for the configuration screen.  Certain extra checking is done
     on the startup screen.  Defaults to false */
  bool startupScreenSemantics;

 public slots:
  void askUserForInputFilename();
  void askUserForOutputFilename();
  void askUserForTemplateFilename();
  void toSettings(); // merges state of dialog box into the settings
  void fromSettings(); // sets state of dialog box from the settings

 protected slots:
  void accept();
  /* internal helper, adds/replaces the appropriate suffix for output filename */
  void resuffixIze();

 private: 

  class DeviceListView : public QListView
  {

    public:
      DeviceListView ( const vector<ComediDevice> & v, 
	       QWidget * parent = 0, const char * name = 0 ); 

      const ComediDevice & selectedDevice() const;

    private:
      const vector<ComediDevice> & devs;
      
  };

  class TextContentsPreviewer: public QTextView, public QFilePreview
  {
    public:
      TextContentsPreviewer (QWidget *w = 0, const char *name = 0);
      virtual void previewUrl (const QUrl & url);

  };

  /* validates this form and returns false if form failed */
  bool validate();


  const Probe & deviceProbe;
  DAQSettings & settings;

  QGridLayout masterGrid; // 5 x 1
  QGroupBox deviceSelectionGroup, templateSelectionGroup, 
            outputFileSelectionGroup;

  QWidget deviceGroupContainer, templateGroupContainer, 
          outputFileGroupContainer;   /* dummy container widgets */

  QGridLayout deviceSelectionGrid/*4 x 3*/, templateSelectionGrid/*3 x 2*/,
              outputFileSelectionGrid /* 3 x 2 */;
  
  QButtonGroup deviceSelectionRadioGroup, outputFileRadioGroup;
  QRadioButton deviceRadio, fileRadio;
  QLabel outputFileRadioLabel;
  DeviceListView deviceTable; 
  QHBox asciiOrBinaryBox;
  QRadioButton asciiRadio, binaryRadio; 
  QLineEdit inputFile, templateFile, outputFile;
  QPushButton browseInputFiles, browseLogTemplates, browseOutputFiles;
  QLabel logPreviewerLabel;
  TextContentsPreviewer logPreviewer;
  QCheckBox showDialogOnStartupChk;
  QPushButton OK;

  static const char * const NDS_ASCII_SUFFIX,
                    * const NDS_BIN_SUFFIX;
#ifdef TEST_CW
  friend int main (int argc, char *argv[], char *envp[]);
#endif
};

#ifdef TEST_CW
int main(int argc, char *argv[], char *envp[]);

#endif

#endif
