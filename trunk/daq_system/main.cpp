/*
 * This file is part of the RT-Linux Multichannel Data Acquisition System
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

#include <qapp.h>
#include <qpushbt.h>
#include <qfont.h>
#include <qwhatsthis.h>
#include <strstream>

#include "text_window.h"
#include "daq_system.h"

int ad_fifo_filedes;                   // RTLinux FIFO file descriptor
int command_line_ai_channels;        //number of channels to acquire data from

const HardwareSpecificStruct hardwareSpecificStruct = //from hardware_specific.h
                              DAQ_HARDWARE_SPECIFIC_STRUCT_INIT; 

const int MainButtonWidth=125;                        // QPushbutton width
const int TitleHeight=35;                        // QPushbutton height
const int PulldownHeight = 20;
const int BtwnMainButtons=10;                          // space between buttons
const int DAQSystemWidth=MainWidgetWidth-2*XBorder;   // width of daq_system
const int DAQSystemHeight=MainWidgetHeight-YBorder-20;// height of daq_system


MainWidget::MainWidget( SharedMemStruct *sh_mem, QWidget *parent, const char *name )
        : QWidget( parent, name )
{
#ifdef DAQ_DEBUG_FLAG
  cout << endl
       << "NUM_AD_CHANNELS_TO_USE: " << NUM_AD_CHANNELS_TO_USE << endl
       << "NUM_AD_CHANNELS: " << NUM_AD_CHANNELS << endl << endl;
#endif  

  CallConfigurationWindow();

  NameOutputFile();

  /****** daq_system 1 *******/
  options.tot_num_channels = options.num_channels; //value from configuration window
  sh_mem->num_ai_chan_in_use = options.tot_num_channels; 
 
  if(options.tot_num_channels%MaxChanPerPage)
    options.tot_pages = options.tot_num_channels/MaxChanPerPage +1; //To make sure there is a page for every graph
  else options.tot_pages = options.tot_num_channels/MaxChanPerPage;
  sh_mem->num_ai_chan_in_use=options.tot_num_channels; 
  //use Qtimer to signal ECG monitor to access FIFO
  //start timer
  timer = new QTimer(this);
  timer->start(1);

  daq_system = new DAQSystem(options, sh_mem, this,"daq_system");
      
  daq_system->setGeometry( XBorder, YBorder+10, 
			   DAQSystemWidth, DAQSystemHeight );

  connect( timer, SIGNAL(timeout()), daq_system, SLOT(SignalMonitor()) );

  quit_now = new QPushButton( "Quit", this, "quit_now" );
  connect( quit_now, SIGNAL(clicked()), 
	   daq_system, SLOT(QuitDAQSystem()) );

  MenuBarSetup();

  daq_system_label = new QLabel(this);
  daq_system_label->setText( "RTLinux Multichannel Data Acquisition System" );
  daq_system_label->setFont( QFont( "Times", 18, QFont::Bold ) );
  daq_system_label->setAlignment( AlignCenter );
  daq_system_label->setGeometry( XBorder, menuBar->height()+BtwnMainButtons, 
				 DAQSystemWidth, TitleHeight);
  cout<< " daq_system_label: " <<daq_system_label->y() <<endl;

}

void MainWidget::MenuBarSetup()
{

  menuBar = new QMenuBar(this, "MenuBar");
  helpMenu = new QPopupMenu(this, "helpMenu");
  helpMenu->insertItem("DAQ System &Help", this, SLOT(Help()), CTRL+Key_H);
  helpMenu->insertItem("&What's This?", daq_system, SLOT(WhatsThis()), CTRL+Key_W);
  helpMenu->insertItem( "Copyright",  this, SLOT(Copyright()));
  menuBar->insertItem( "Help", helpMenu );
  menuBar->insertItem(quit_now);

}
void MainWidget::Help()
{

  TextWindow *help_text_window =
    new TextWindow(0, "DAQ System v" VERSION_NUM " Copyright and License", overview_text);

  help_text_window->setCaption("DAQ System v" VERSION_NUM " Overview");
  help_text_window->setGeometry(QRect(200,200,600,400));
  help_text_window->show();
}

void MainWidget::Copyright()
{
  TextWindow *about_text_window =
    new TextWindow(0, "DAQ System Copyright and License", copyright_text);
  about_text_window->setCaption("DAQ System Copyright and License");
  about_text_window->setGeometry(QRect(200,200,600,400));
  about_text_window->show();

}


void MainWidget::NameOutputFile() { //name ascii and binary output files
  bool first_time=1;  //is (1) or is not (0) 1st time through while loop
  QFile temp_filename_ascii("");
  QFile temp_filename_bin("");
  QFileDialog fd_out( "data", "*", this, 0, TRUE  );

  while (temp_filename_ascii.exists()||
	 temp_filename_bin.exists()||
	 first_time) { //repeat til a NEW file chosen
    if (!first_time) //don't print message first time
      QMessageBox::information( this, "File already exists!",
		   "Please choose a NEW filename (one that doesn't exist).\n");
    first_time=0;   //no longer first time

    //file dialog
    QString window_caption=
      "Choose a filename trunk (.dat and .bin will be appended)";
    fd_out.setCaption(window_caption);
    fd_out.setMode(QFileDialog::AnyFile);

    if (!(fd_out.exec() == QDialog::Accepted)) {
      options.output_ascii = "data/cancelled.dat";
      options.output_binary = "data/cancelled.bin";
      //display in main window caption so user knows which file was opened
      this->setCaption("output file = cancelled.dat/bin");
      return;
    }
    else { //user entered a filename
      //temp_filename to test if file exists (above)
      temp_filename_ascii.setName(fd_out.selectedFile()+".dat");
      temp_filename_bin.setName(fd_out.selectedFile()+".bin");
    }
  }

  //name ascii and binary output files
  options.output_ascii = fd_out.selectedFile() + ".dat";
  options.output_binary = fd_out.selectedFile() +".bin";

  //display file in main window caption so user knows which file was opened
  this->setCaption("output file base = " + fd_out.selectedFile());
}

void MainWidget::CallConfigurationWindow()
{
  QColorGroup ConfigColors(white,darkCyan,lightGray,
			   gray, darkGray,white,
			   white,color0,darkBlue);
  QPalette ConfigPalette;
  ConfigPalette.setActive(ConfigColors);
  ConfigPalette.setInactive(ConfigColors);
  ConfigPalette.setDisabled(ConfigColors);
  
  ConfigurationWindow *configuration_window =
    new ConfigurationWindow(0, "Data Acquisition Configuration",options);
  configuration_window->setPalette(ConfigPalette);
  configuration_window->setCaption("Please detail the data acquisition structure");
  configuration_window->setGeometry(QRect(200,100,550,520));
  configuration_window->exec();
}

int main( int argc, char **argv )
{

  command_line_ai_channels = 1;

  QApplication a( argc, argv );

  /* set up our rt-fifo id's from the include file defines */
  const unsigned int readFifos[NUM_GET_FIFOS] = GET_FIFOS; 
  const unsigned int putFifos[NUM_PUT_FIFOS] = PUT_FIFOS;

  /* open the fifos (returns nonzero on error, prints errors via perror) 
     currently we only use ONE read-only fifo, /dev/rtf0,
   */
  if (openFifos (readFifos, 
		 NUM_GET_FIFOS, 
		 putFifos, 
		 NUM_PUT_FIFOS, 
		 &ad_fifo_filedes, 
		 NULL)
      != 0) {
    fprintf (stderr,
	     "It is possible that %s%s is not loaded or did "
	     "not load correctly.\n", RT_PROCESS_MODULE_NAME, ".o");
    exit(1);
  }

  

  /* Shared Memory communicates with RT process -- use mbuff_attach()
     in userspace, and mbuff_alloc in kernel space */   
  SharedMemStruct *sh_mem =
    (SharedMemStruct *)mbuff_attach(RT_PROCESS_SHM_NAME, 
				    sizeof(SharedMemStruct));
  if (!sh_mem) {
    fprintf(stderr, 
	    "Failure: Cannot attach to shared memory segment '%s'. "
	    "Make sure that you have permission to access /dev/mbuff and "
	    "that mbuff.o and %s%s are loaded.\n", 
	    RT_PROCESS_SHM_NAME, RT_PROCESS_MODULE_NAME, ".o");
    exit(1);
  }

  MainWidget w(sh_mem);
  // xstart, ystart, width, height
  w.setGeometry( 5, 5, MainWidgetWidth, MainWidgetHeight );  

  a.setMainWidget( &w );
  w.show();

  int ret = a.exec();

  // detach from shared memory
  mbuff_detach(RT_PROCESS_SHM_NAME,sh_mem);

  // close our 1 fifo
  close(ad_fifo_filedes);

  return ret;

}


