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
#ifndef _OUTPUT_FILE_W_H
#define _OUTPUT_FILE_W_H

#include <qwidget.h>
#include <qstring.h>
#include "common.h"
#include "daq_settings.h"

class QGroupBox;
class QGridLayout;
class QButtonGroup;
class QRadioButton;
class QLineEdit;
class QPushButton;


/* A Widget used in the configuration screen and (maybe) other places
   to specify an output file and format */

class OutputFileW : public QWidget
{
  Q_OBJECT

public:
  OutputFileW(DAQSettings * settings = 0, 
               QWidget * parent = 0, const char * name = 0, WFlags f = 0);
  ~OutputFileW();

  QString fileName() const;
  DAQSettings::DataFileFormat fileFormat() const;
  void setFileName(const QString & fn);
  void setFileFormat(DAQSettings::DataFileFormat fmt);

  static const char * const NDS_ASCII_SUFFIX,
                    * const NDS_BIN_SUFFIX;

public slots:
  void toSettings();
  void fromSettings();

private slots:
  void askUserForOutputFilename();
  void resuffixIze();


protected:
  void showEvent (QShowEvent *);

private:

  QGroupBox *outputFileSelectionGroup;
  QGridLayout *outputFileSelectionGrid /* 3 x 2 */;
  QButtonGroup *outputFileRadioGroup;
  QRadioButton *asciiRadio, *binaryRadio; 
  QLineEdit *outputFile;
  QPushButton *browseOutputFiles;

  DAQSettings * settings;
};
#endif
