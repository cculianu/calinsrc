/*
 * This file is part of the "analyze_ecgs" software  package.
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

#ifndef _CONFIGURATIONWINDOW_H
#define _CONFIGURATIONWINDOW_H

#include "analyze_ecgs.h"

#include <iostream.h>   
#include <fstream.h>    
#include <stdlib.h> //for random()    

#include <qdialog.h>
#include <qlayout.h> 
#include <qlabel.h> 
#include <qmultilinedit.h> 
#include <qpushbutton.h>
#include <qscrollbar.h>

//****** ConfigurationWindow

class ConfigurationWindow : public QDialog //inherits QDialog class
{
  Q_OBJECT   //Q_OBJECT macro required for signals and slots
    public:
  ConfigurationWindow(GUIs *parent, const char *name, ConfParams& conf_params);
  ~ConfigurationWindow(); //destructor
 private:
  QVBoxLayout *top_layout;           //overall layout
  QMultiLineEdit *description_label; //displays description of ConfigurationWindow

  QPushButton *binary_or_ascii_input_button;
  QLabel *binary_or_ascii_input_text;
  QPushButton *binary_or_ascii_output_button;
  QLabel *binary_or_ascii_output_text;
  QPushButton *annotated_or_raw_button;
  QLabel *annotated_or_raw_text;
  QLabel *time_col_text;           //text showing column holding absolute time
  QPushButton *time_col_button;    //scrolls time_col
  QLabel *num_signals_text;        //text showing num. of input channels
  QScrollBar *num_signals_scroller;//scrolls num. of input channels
  QLabel *sample_rate_text;        //text showing data sample_rate
  QScrollBar *sample_rate_scroller;//scrolls sample_rate
  QPushButton *done_button;     //finish MLP

 public:
  ConfParams& conf_params;           //for OutputMLP access of input_record

 public slots:
    void    ToggleBinaryOrASCIIinput();
    void    ToggleBinaryOrASCIIoutput();
    void    ToggleAnnotatedOrRaw();
    void    ToggleTimeColumn();
    void    ScrollNumSignals(int); 
    void    ScrollSampleRate(int); 

    void    QuitConfigurationWindow();          //quit 
};

#endif
