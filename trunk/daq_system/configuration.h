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


#include <iostream.h>   
#include <fstream.h>    
#include <stdlib.h> //for random()    
#include <string.h>

#include <qdialog.h>
#include <qgroupbox.h> 
#include <qhbox.h>
#include <qlabel.h> 
#include <qmultilinedit.h> 
#include <qpushbutton.h>
#include <qradiobutton.h>
#include <qscrollbar.h>
#include <qfiledialog.h>

class QCheckBox;          //shortcut to avoid including files
class QButtonGroup; 

typedef struct {

  int num_channels;
  int tot_num_channels;
  bool show_spike;
  int sec_to_display;
  QString template_file; 
  QPalette configPalette;           //The configuration window palette has been set up by the main program - LG
  int page_number;
  int tot_pages;
  QString output_ascii;
  QString output_binary;
  bool use_output_ascii;
}GraphingOptions;

const int NumSecondsOptions = 4;
//****** ConfigurationWindow

class ConfigurationWindow : public QDialog //inherits QDialog class
{
  Q_OBJECT   //Q_OBJECT macro required for signals and slots
    public:
  ConfigurationWindow(QWidget *parent, const char *name, GraphingOptions &graph_options);
  ~ConfigurationWindow(); //destructor

 private:

  QGroupBox       *control_box;           //overall layout
  QLabel          *title; //displays description of ConfigurationWindow
  QPushButton     *binary_or_ascii_output_button;
  QLabel          *binary_or_ascii_output_text;
  QLabel          *num_signals_text;        //text showing num. of input channels
  QScrollBar      *num_signals_scroller;//scrolls num. of input channels
  QCheckBox       *spike;
  QButtonGroup    *seconds_group;
  QRadioButton    *seconds_buttons[NumSecondsOptions];
  QScrollBar      *seconds_scroller;
  QLabel          *seconds_text;
  QPushButton     *template_select;
  QRadioButton    *use_template;
  QRadioButton    *blank;
  QLabel          *file_name;                    //displays the file name chosen 
  QButtonGroup      *data_type_buttons; //for choosing ascii or binary output
  QRadioButton      *ascii_button;      //for selecting ascii output
  QRadioButton      *binary_button;     //for selecting binary output
  QPushButton     *OK;     //finish
  GraphingOptions  *config_options;  

  private slots:
    void           changeSpike();
  
  public slots:
    void           ScrollNumSignals(int value);
    void           ChangeOutputMode(int value); 
    void           RadioSeconds(int value);
    void           ToggleSelect();                          //Determines whether the template select is enabled based on whether or not the use has selected to use the default template file - LG
    void           SelectFile();                       //Opens a QFileDialog to select the template file
    void           QuitConfigurationWindow();          //quit 
};


#endif
