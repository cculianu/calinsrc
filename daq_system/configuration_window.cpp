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

#include "configuration_window.h"
#include "main_application_help.h"

ConfigurationWindow::ConfigurationWindow(GUIs *parent, const char *name,
					 ConfParams& conf_params_in)
  : QDialog(parent, name, TRUE), conf_params(conf_params_in)
  //inherits QDialog so that user cannot continue with other parts of
  //the program until this dialog is exited
{

  QBrush lightGrayBrush=QBrush(lightGray);
  QBrush redBrush=QBrush(red);
  QBrush whiteBrush=QBrush(white);
  QColorGroup ScrollerTextColors(redBrush,whiteBrush,whiteBrush,lightGrayBrush,
				 whiteBrush,redBrush,redBrush,whiteBrush,
				 lightGrayBrush);
  QPalette ScrollerTextPalette(ScrollerTextColors,ScrollerTextColors,
			       ScrollerTextColors);

  top_layout = new QVBoxLayout(this, 10);

  description_label = new QMultiLineEdit(this);
  description_label->setReadOnly(TRUE);
  description_label->setText(configuration_window_text);

  top_layout->addWidget(description_label,2);
  top_layout->addSpacing(20);

  //************* widgets for setting number of nodes
  //binary_or_ascii_input
  conf_params.binary_or_ascii_input=BINARY_DATA;
  binary_or_ascii_input_text = new QLabel(this);
  QString binary_or_ascii_input_text_a, binary_or_ascii_input_text_b;
  binary_or_ascii_input_text_a = "Binary or ASCII input datafile = ";
  binary_or_ascii_input_text_b = "binary";
  binary_or_ascii_input_text->setText(binary_or_ascii_input_text_a+binary_or_ascii_input_text_b);
  binary_or_ascii_input_text->setFont(QFont("Times",18,QFont::Normal));
  binary_or_ascii_input_text->setAlignment(AlignHCenter|AlignBottom);
  binary_or_ascii_input_text->setPalette(ScrollerTextPalette);
  binary_or_ascii_input_button = new QPushButton( "Toggle", this );
  connect( binary_or_ascii_input_button,SIGNAL(pressed()),
	   this, SLOT(ToggleBinaryOrASCIIinput()) );

  //binary_or_ascii_input
  conf_params.binary_or_ascii_output=BINARY_DATA;
  binary_or_ascii_output_text = new QLabel(this);
  QString binary_or_ascii_output_text_a, binary_or_ascii_output_text_b;
  binary_or_ascii_output_text_a = "Binary or ASCII output datafile = ";
  binary_or_ascii_output_text_b = "binary";
  binary_or_ascii_output_text->setText(binary_or_ascii_output_text_a+binary_or_ascii_output_text_b);
  binary_or_ascii_output_text->setFont(QFont("Times",18,QFont::Normal));
  binary_or_ascii_output_text->setAlignment(AlignHCenter|AlignBottom);
  binary_or_ascii_output_text->setPalette(ScrollerTextPalette);
  binary_or_ascii_output_button = new QPushButton( "Toggle", this );
  connect( binary_or_ascii_output_button,SIGNAL(pressed()),
	   this, SLOT(ToggleBinaryOrASCIIoutput()) );

  //annotated_or_raw
  conf_params.annotated=1;
  annotated_or_raw_text = new QLabel(this);
  QString annotated_or_raw_text_a, annotated_or_raw_text_b;
  annotated_or_raw_text_a = "Annotated or Raw datafile = ";
  annotated_or_raw_text_b = "annotated";
  annotated_or_raw_text->
    setText(annotated_or_raw_text_a+annotated_or_raw_text_b);
  annotated_or_raw_text->setFont(QFont("Times",18,QFont::Normal));
  annotated_or_raw_text->setAlignment(AlignHCenter|AlignBottom);
  annotated_or_raw_text->setPalette(ScrollerTextPalette);
  annotated_or_raw_button = new QPushButton( "Toggle", this );
  connect( annotated_or_raw_button,SIGNAL(pressed()),
	   this, SLOT(ToggleAnnotatedOrRaw()) );

  //time_col
  conf_params.time_col=1;
  time_col_text = new QLabel(this);
  QString time_col_text_a, time_col_text_b;
  time_col_text_a = "First column contains time or data? = ";
  time_col_text_b = "TIME";
  time_col_text->setText(time_col_text_a+time_col_text_b);
  time_col_text->setFont(QFont("Times",18,QFont::Normal));
  time_col_text->setAlignment(AlignHCenter|AlignBottom);
  time_col_text->setPalette(ScrollerTextPalette);
  time_col_button = new QPushButton( "Toggle", this );
  connect( time_col_button,SIGNAL(pressed()),
	   this, SLOT(ToggleTimeColumn()) );

  //num_signals
  num_signals_text = new QLabel(this);
  QString num_signals_text_a, num_signals_text_b;
  num_signals_text_a = "Number of channels (signals) = ";
  num_signals_text_b.sprintf("%d",conf_params.num_signals);
  num_signals_text->setText(num_signals_text_a+num_signals_text_b);
  num_signals_text->setFont(QFont("Times",18,QFont::Normal));
  num_signals_text->setAlignment(AlignHCenter|AlignBottom);
  num_signals_text->setPalette(ScrollerTextPalette);
  num_signals_scroller = new QScrollBar(1,MaxNumSignals,//range
			      1, MaxNumSignals,         //line/page steps
			      conf_params.num_signals,  //initial value
			      QScrollBar::Horizontal,   //orientation
			      this, "scrollbar" );
  connect( num_signals_scroller,SIGNAL(valueChanged(int)),
	   this, SLOT(ScrollNumSignals(int)) );

  //sample_rate
  sample_rate_text = new QLabel(this);
  QString sample_rate_text_a, sample_rate_text_b;
  sample_rate_text_a = "Data sampling rate = ";
  sample_rate_text_b.sprintf("%d",conf_params.sample_rate);
  sample_rate_text->setText(sample_rate_text_a+sample_rate_text_b);
  sample_rate_text->setFont(QFont("Times",18,QFont::Normal));
  sample_rate_text->setAlignment(AlignHCenter|AlignBottom);
  sample_rate_text->setPalette(ScrollerTextPalette);
  sample_rate_scroller = new QScrollBar(1,2000,//range
			      50,100,         //line/page steps
		 	      conf_params.sample_rate,  //initial value
			      QScrollBar::Horizontal,   //orientation
			      this, "scrollbar" );
  connect( sample_rate_scroller,SIGNAL(valueChanged(int)),
	   this, SLOT(ScrollSampleRate(int)) );

  QGridLayout *nodes_layout = new QGridLayout(6,8);
  top_layout->addLayout(nodes_layout,3);

  int which_row=0;
  nodes_layout->addMultiCellWidget(binary_or_ascii_input_button,
				   which_row,which_row,0,1);
  nodes_layout->addMultiCellWidget(binary_or_ascii_input_text,
				   which_row,which_row,2,8);

  which_row++;
  nodes_layout->addMultiCellWidget(binary_or_ascii_output_button,
				   which_row,which_row,0,1);
  nodes_layout->addMultiCellWidget(binary_or_ascii_output_text,
				   which_row,which_row,2,8);

  which_row++;
  nodes_layout->addMultiCellWidget(annotated_or_raw_button,
				   which_row,which_row,0,1);
  nodes_layout->addMultiCellWidget(annotated_or_raw_text,
				   which_row,which_row,2,8);

  which_row++;
  nodes_layout->addMultiCellWidget(time_col_button,
				   which_row,which_row,0,1);
  nodes_layout->addMultiCellWidget(time_col_text,
				   which_row,which_row,2,8);

  which_row++;
  nodes_layout->addMultiCellWidget(num_signals_scroller,
				   which_row,which_row,0,1);
  nodes_layout->addMultiCellWidget(num_signals_text,
				   which_row,which_row,2,8);

  which_row++;
  nodes_layout->addMultiCellWidget(sample_rate_scroller,
				   which_row,which_row,0,1);
  nodes_layout->addMultiCellWidget(sample_rate_text,
				   which_row,which_row,2,8);

  top_layout->addSpacing(20);

  done_button = new QPushButton("OK", this);
  done_button->setMaximumSize(120,40);
  done_button->setFont(QFont("Times",18,QFont::Bold));
  done_button->setDefault(TRUE);
  connect(done_button, SIGNAL(clicked()),
	  this, SLOT(QuitConfigurationWindow()));

  QGridLayout *grid = new QGridLayout(1,5);
  top_layout->addLayout(grid,3);
  grid->addWidget(done_button,1,2);

  top_layout->activate();

}

ConfigurationWindow::~ConfigurationWindow(){
  delete done_button;
  delete top_layout;
};

void ConfigurationWindow::ToggleBinaryOrASCIIinput() {
  QString binary_or_ascii_input_text_a, binary_or_ascii_input_text_b;
  binary_or_ascii_input_text_a = "Binary or ASCII input datafile = ";

  if (conf_params.binary_or_ascii_input==ASCII_DATA) {
    conf_params.binary_or_ascii_input=BINARY_DATA;
    binary_or_ascii_input_text_b = "binary";
  }
  else {
    conf_params.binary_or_ascii_input=ASCII_DATA;
    binary_or_ascii_input_text_b = "ASCII";
  }
  binary_or_ascii_input_text->
    setText(binary_or_ascii_input_text_a+binary_or_ascii_input_text_b);
}

void ConfigurationWindow::ToggleBinaryOrASCIIoutput() {
  QString binary_or_ascii_output_text_a, binary_or_ascii_output_text_b;
  binary_or_ascii_output_text_a = "Binary or ASCII output datafile = ";

  if (conf_params.binary_or_ascii_output==ASCII_DATA) {
    conf_params.binary_or_ascii_output=BINARY_DATA;
    binary_or_ascii_output_text_b = "binary";
  }
  else {
    conf_params.binary_or_ascii_output=ASCII_DATA;
    binary_or_ascii_output_text_b = "ASCII";
  }
  binary_or_ascii_output_text->
    setText(binary_or_ascii_output_text_a+binary_or_ascii_output_text_b);
}

void ConfigurationWindow::ToggleAnnotatedOrRaw() {
  QString annotated_or_raw_text_a, annotated_or_raw_text_b;
  annotated_or_raw_text_a = "Annotated or Raw datafile = ";

  if (conf_params.annotated) {
    conf_params.annotated=0;
    annotated_or_raw_text_b = "raw";
  }
  else {
    conf_params.annotated=1;
    annotated_or_raw_text_b = "annotated";
  }
  annotated_or_raw_text->
    setText(annotated_or_raw_text_a+annotated_or_raw_text_b);
}

void ConfigurationWindow::ToggleTimeColumn() {
  QString time_col_text_a, time_col_text_b;
  time_col_text_a = "First column contains time or data? = ";

  if (conf_params.time_col) {
    conf_params.time_col=0;
    time_col_text_b = "DATA";
  }
  else {
    conf_params.time_col=1;
    time_col_text_b = "TIME";
  }
  time_col_text->
    setText(time_col_text_a+time_col_text_b);
}

void ConfigurationWindow::ScrollNumSignals(int value) {
  conf_params.num_signals=value;
  QString num_signals_text_a, num_signals_text_b;
  num_signals_text_a = "Number of channels (signals) = ";
  num_signals_text_b.sprintf("%d",conf_params.num_signals);
  num_signals_text->
    setText(num_signals_text_a+num_signals_text_b);
}

void ConfigurationWindow::ScrollSampleRate(int value) {
  conf_params.sample_rate=value;
  QString sample_rate_text_a, sample_rate_text_b;
  sample_rate_text_a = "Data sampling rate = ";
  sample_rate_text_b.sprintf("%d",conf_params.sample_rate);
  sample_rate_text->
    setText(sample_rate_text_a+sample_rate_text_b);
}

void ConfigurationWindow::QuitConfigurationWindow() {
  delete this; //delete ConfigurationWindow
}

