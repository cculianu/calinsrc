/*
 * This file is part of the RT-Linux Multichannel Data Acquisition System
 *
 * Copyright (C) 1999,2000 David Christini
 * Copyright (c) 2001 David Christini, Lyuba Golub
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

#include <qwidget.h>
#include <qpainter.h>
#include <qgroupbox.h>
#include <qbuttongroup.h>
#include <qcheckbox.h>
#include <qaccel.h>
#include <stdio.h>
#include "configuration.h"
#include "rt_process.h"

ConfigurationWindow::ConfigurationWindow(QWidget *parent, const char *name, GraphingOptions &graph_options)  
  : QDialog(parent, name, TRUE)
  //inherits QDialog so that user cannot continue with other parts of
  //the program until this dialog is exited
{
  config_options = &graph_options; //Configuration window gets a pointer to the address of the main program's graphing options - LG

  QColorGroup ScrollerTextColors(darkBlue,gray,lightGray,darkGray,gray,black,black,darkCyan,lightGray);
  QPalette ScrollerTextPalette(ScrollerTextColors,ScrollerTextColors,
			       ScrollerTextColors);

  QColorGroup TitleColors(darkCyan,gray,lightGray,darkGray,gray,black,black,darkCyan,lightGray);
  QPalette TitlePalette(TitleColors,TitleColors,
			       TitleColors);

  control_box = new QGroupBox(9, Vertical, this, "Layout");
  control_box->setGeometry(25, 25, 500, 470);
  control_box->setAlignment(AlignLeft);
  control_box->setPalette(ScrollerTextPalette);
  control_box->setFrameStyle(QFrame::Panel|QFrame::Raised);

  title = new QLabel(control_box);
  title->setFont(QFont("Times",18,QFont::Normal));
  title->setAlignment(AlignHCenter|AlignTop);
  title->setText("Data Acquisition System Configuration");
  title->setPalette(TitlePalette);

  QGroupBox *channels = new QGroupBox(4, Vertical, control_box, "Channels");
  channels->setAlignment(AlignLeft);
  channels->setPalette(ScrollerTextPalette);
  num_signals_scroller = new QScrollBar(1, NUM_AD_CHANNELS_TO_USE, //range
					1, NUM_AD_CHANNELS_TO_USE, //line/page steps
					1, //initial value  
					QScrollBar::Horizontal, //orientation 
					channels, "Signal Number");
  num_signals_scroller->setMinimumSize(150, 20);
  num_signals_scroller->setMaximumSize(150, 20);

  num_signals_text = new QLabel(channels);
  ScrollNumSignals(1); //Sets up initial value
  num_signals_text->setFont(QFont("Times",14,QFont::Normal));
  num_signals_text->setAlignment(AlignLeft|AlignBottom);
  num_signals_text->setPalette(ScrollerTextPalette);

  connect(num_signals_scroller, SIGNAL(valueChanged(int)), this, SLOT(ScrollNumSignals(int)));

  spike = new QCheckBox("Show Spike Indicator", channels, "spike");
  spike->setChecked(TRUE);
  config_options->show_spike=TRUE;

  config_options->use_output_ascii = FALSE; //default is binary
  data_type_buttons = new QButtonGroup(2, Horizontal, "Output Data Type:", channels, "data_type_buttons");
  data_type_buttons->setFrameStyle( QFrame::NoFrame );
  data_type_buttons->setPalette(ScrollerTextPalette);
  ascii_button = new QRadioButton ("ascii",data_type_buttons,"ascii_type_button");
  ascii_button->setChecked(config_options->use_output_ascii);
  binary_button = new QRadioButton ("binary",data_type_buttons,"binary_type_button");
  binary_button->setChecked(!(config_options->use_output_ascii));

  connect( data_type_buttons, SIGNAL(clicked(int)), 
	   this, SLOT(ChangeOutputMode(int)) );

  seconds_group = new QButtonGroup(4, Vertical, "Seconds to Display:", control_box, "seconds_group");
  seconds_group->setPalette(ScrollerTextPalette);
  for(int iii=0; iii<NumSecondsOptions; iii++)
    {
      QString name;
      seconds_buttons[iii]=new QRadioButton(seconds_group);
      
    }
  seconds_buttons[0]->setText("5");
  seconds_buttons[0]->setChecked(TRUE);
  seconds_buttons[1]->setText("10");
  seconds_buttons[2]->setText("20");
  seconds_buttons[3]->setText("25");
  RadioSeconds(0); //Sets the default seconds to display options - LG
  connect(seconds_group, SIGNAL(clicked(int)), this, SLOT(RadioSeconds(int)));
  
  QGroupBox *template_group = new QGroupBox(2, Vertical, "Log Template", control_box, "Log Template");

  template_group->setPalette(ScrollerTextPalette);
  QButtonGroup *template_buttons = new QButtonGroup(2, Horizontal, template_group, "template_buttons");
  template_buttons->setFrameStyle(QFrame::NoFrame);
  use_template = new QRadioButton("Use Template", template_buttons, "template");
  use_template->setChecked(TRUE);
  
  blank = new QRadioButton("Blank", template_buttons, "blank");
  QHBox *template_box =  new QHBox(template_group, "template_box", 0, TRUE);
  template_select = new QPushButton(template_box);
  template_select->setText("Choose Filename");
  
  file_name = new QLabel(template_box);
  file_name->setFixedSize(300, 30);
  file_name->setFont(QFont("Times",14,QFont::Normal));
  file_name->setAlignment(AlignLeft|AlignBottom);
  file_name->setBackgroundColor(white);
  file_name->setPalette(ScrollerTextPalette);
  file_name->
    setFrameStyle( QFrame::WinPanel | QFrame::Sunken );
  ToggleSelect(); //Sets the default template options -LG
  connect(use_template, SIGNAL(clicked()), this, SLOT(ToggleSelect()));
  connect(blank, SIGNAL(clicked()), this, SLOT(ToggleSelect()));
  connect(template_select, SIGNAL(clicked()), SLOT(SelectFile()));

  OK = new QPushButton("OK", control_box, "OK");
  OK->setFixedSize(70, 25);
  OK->setPalette(ScrollerTextPalette);
  connect(spike, SIGNAL(clicked()), this, SLOT(changeSpike()));
  connect(OK, SIGNAL(clicked()), this, SLOT(QuitConfigurationWindow()));
  QAccel *accel = new QAccel(this, "accel");
  accel->connectItem(accel->insertItem(QAccel::Key_Enter, 111), OK, SLOT(animateClick()));
  accel->connectItem(accel->insertItem(QAccel::Key_Return, 222), OK, SLOT(animateClick()));

  QLabel *version = new QLabel(control_box);
  version->setText("v"VERSION_NUM);
  version->setPalette(ScrollerTextPalette);
  version->setFixedSize(control_box->width()-40, 30);
  version->setAlignment(AlignRight);
}


void ConfigurationWindow::ScrollNumSignals(int value)
 {
   config_options->num_channels = value;  
   QString num_signals_text_a, num_signals_text_b;
   num_signals_text_a = "Channels: ";
   num_signals_text_b.sprintf("%d",config_options->num_channels);
   num_signals_text->
     setText(num_signals_text_a+num_signals_text_b);
 }

void ConfigurationWindow::ChangeOutputMode(int which_type) {

  config_options->use_output_ascii= ascii_button->isChecked(); //toggle output_ascii
  which_type=0; //purpose of this line is to elim. warning: which_type not used
}

void ConfigurationWindow::RadioSeconds(int id)
{
  switch(id)
    {
    case 0:
      config_options->sec_to_display = 5;
      break;
    case 1:
      config_options->sec_to_display = 10;
      break;
    case 2:
      config_options->sec_to_display = 20;
      break;
    case 3:
      config_options->sec_to_display = 25;
      break;
    }

}

void ConfigurationWindow::ToggleSelect()
{
  if(blank->isChecked())
    {
      config_options->template_file="";
      file_name->setText(config_options->template_file);
      template_select->setEnabled(FALSE);
    }
  else 
    {
      template_select->setEnabled(TRUE); //If the default template box is checked, the template select button is disabled - LG
      config_options->template_file = "data/default.log";
      file_name->setText(config_options->template_file);
    }
}

void ConfigurationWindow::SelectFile()
{

  QFileDialog templ( "data", "*", this, 0, TRUE  ); 
  QString window_caption= 
    "Choose a template filename"; 
  templ.setCaption(window_caption); 
  templ.setMode(QFileDialog::AnyFile); 
  if (!(templ.exec() == QDialog::Accepted)) return; 
  config_options->template_file = templ.selectedFile(); 
  file_name->setText(config_options->template_file); 
}

void ConfigurationWindow::changeSpike()
{
  if(spike->isChecked()) config_options->show_spike = TRUE;
  else config_options->show_spike = FALSE;
} 


void ConfigurationWindow::QuitConfigurationWindow(){
  this->close(TRUE);
}

ConfigurationWindow::~ConfigurationWindow(){

}
