/*
 * This file is part of the RT-Linux Multichannel Data Acquisition System
 *
 * Copyright (C) 1999,2000 David Christini
 * Copyright (C) 2001 David Christini, Lyuba Golub
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

#ifndef _DAQSystem_H
#define _DAQSystem_H

#include <stdlib.h>
#include <math.h>
#include <iostream.h>    //for cout
#include <fstream.h>     //for fout
#include <stdio.h>
#include <fcntl.h>     //for open()
#include <sys/types.h> //for open() types
#include <sys/stat.h>  //for open() types
#include <sys/time.h>
#include <unistd.h>
#include <sys/mman.h>

#include <qwidget.h>
#include <qbuttongroup.h> 
#include <qfile.h> 
#include <qfiledialog.h> 
#include <qframe.h>
#include <qcombo.h>
#include <qhbox.h>
#include <qvbox.h>
#include <qevent.h>
#include <qpushbt.h>
#include <qmessagebox.h>
#include <qscrbar.h>
#include <qlcdnum.h>
#include <qlabel.h> 
#include <qpalette.h> 
#include <qdatetm.h>
#include <qpainter.h>
#include <qpaintd.h> 
#include <qpixmap.h> 
#include <qradiobutton.h> 
#include <qsignalmapper.h> 
#include <qtimer.h>
#include <qgroupbox.h>
#include <qlayout.h>
#include <qmultilineedit.h>
#include <qtextstream.h>
#include <qmenubar.h>
#include <qpopupmenu.h>
#include <qwhatsthis.h>
#include <qtextbrowser.h>
#include <qstylesheet.h>

#include "rt_process.h"
#include "fifos.h"
#include "configuration.h"
#include "help_text.h"

/********************************** Constants  **************************/

extern int ad_fifo_filedes;       // file descriptor for (currently) /dev/rtf0
extern const HardwareSpecificStruct hardwareSpecificStruct; // setup in hardware_specific.h and defined globally in main.cpp

const int MaxPages = 4;  //Number of DAQ system pages the program can have with 8 channels per page. -LG 
const int MaxChanPerPage=4; //Maximum number of graphs that can be displayed on one page - LG
const int ChanToShow = 3;
const int MainWidgetWidth=1250;                         //width of MainWidget
const int MainWidgetHeight=900;                         //height of MainWidget
const int XBorder=10;             //x border between MainWidget and daq_system
const int YBorder=60;             //y border between MainWidget and daq_system
const int GraphWidthInPixels=1000; 
const int GraphHeightInPixels=300;
/*const int GraphXPosition=(MainWidgetWidth-2*XBorder-GraphWidthInPixels)/2; //Might have to be changed to fit buttons on the right side*/

const int ButtonHeight=22;          //height of buttons and related objects
const int LabelWidth=107;
const int ButtonWidth=55;
const int BoxWidth=120;
const int BtwnButtons=5;            //horizontal space between buttons
const int SpaceAboveGraph=3;        //space above graph (below buttons&labels)
const int AxisLabelWidth=70;        //width y-axis labels for AV,VA,g graphs
const int AxisFontSize=14;          //width y-axis labels for AV,VA,g graphs - changed by LG from 18 
const int GraphXPosition = XBorder + AxisLabelWidth;
const int MaxSecondsToDisplay = 25;       //# seconds of ECG trace to display
const int MaxPointsPerScreen = 
MaxSecondsToDisplay*RT_HZ;//num pts plotted per screen: 5 seconds
const int MaxReplotsPerSec = 20;       //times per sec that ECG graph is refreshed
const int MaxPixmapPerScreen = MaxReplotsPerSec*MaxSecondsToDisplay; //num of pm 

const int SuggestedVerticalBlocksPerScreen=10; //# X divisions per screen
const int HorizontalBlocksPerScreen=4; //# Y divisions per screen
const int NumYAxisTicks=HorizontalBlocksPerScreen-1;//# y-axis ticks per graph

const int SpikeIndicatorDuration=RT_HZ/10; //duration of "***"
//const QColor BackGroundColor = QColor::darkGray;

/********************************** IntervalGraphs **************************/
//****** Graphs
/* encapsulates a graph, plus stores aux. widgets */
class GraphParams: public QWidget { 
 public:
  /* Constructor -- note that all values have defaults */
  GraphParams
    (     
     QWidget *parent = 0, 	   
     QString label = "", 	   
     WFlags f = 0,
     int max_pixel = 0,
     int x_position = 0, 
     int y_position = 0,
     int width = 0, 
     int height = 0, 
     bool pause_display = FALSE,
     int turn_off_spike_indicator = 0 
    ) : 
    QWidget(parent, ((const char *)label), f),
    label(label),
    max_pixel(max_pixel),
    x_position(x_position),
    y_position(y_position),
    width(width),
    height(height),
    pause_display(pause_display),
    turn_off_spike_indicator(turn_off_spike_indicator) {}
    

  QString label;
  int  max_pixel;
  int  x_position;
  int  y_position;
  int  width;
  int  height;
  int  which_signal;    // which channel - LG
  int  which_graph;     // which graph on the page -LG
  int  page_number;
  bool display_graph;   /* whether or not graph is shown - depends 
			   on what page is currently on the screen - LG */

  bool pause_display;
  int turn_off_spike_indicator;
  int data[MaxPointsPerScreen];          //vector holding data

  QLabel *y_axis_tick[NumYAxisTicks];    //min,mid,max labels
  QLabel *monitor_label;                 //monitor label 

  QVBox        *graph_controls;          //Box to arrange pause buttons, gain pulldown menu, etc.
  QGroupBox    *spike_frame;             //Frame with "Spike Indicator" label
  QVBox        *spike_box;               //Box to arrange spike indicator properties
  QHBox        *pause_group;             //Box that holds the "Pause" and "Pause All" buttons
  QLabel       *pause_label;             //Label indicating pause options
  QPushButton  *pause_button;            //button to pause plotting
  QLabel       *pause_button_label;      //label for paused/not paused
  QPushButton  *pause_all;               //pauses all displays - LG
  QComboBox    *gain_pulldown;           //pulldown box for channel_gain
  QLabel       *gain_pulldown_label;     //label for gain_pulldown
  QButtonGroup *polarity_group;          //holds the plus and minus polarity buttons
  QLabel       *polarity_button_label;   //label for button
  QPushButton  *plus_button;             //Switches spike polarity to positive
  QPushButton  *minus_button;            //Switches spike polarity to negative
  QLabel       *scroller_label;          //label for blanking
  QScrollBar   *threshold_scroller;      //scroller for spike_threshold
  QScrollBar   *blanking_scroller;       //scroller for spike_blanking
  QLabel       *blanking_scroller_label; //label for blanking
  QLabel       *spike_indicator;         // ** indicates a spike
  QLabel       *spike_indicator_label;   //labels indicator

};

/***************************** DAQSystem ****************************/
//DAQSystem
class DAQSystem : public QFrame {
  Q_OBJECT

    public:
  DAQSystem( GraphingOptions &options, SharedMemStruct *sh_mem_param, QWidget *parent=0, const char *name=0); //constructor
  QRect  SpikeBox();              //returns the spike indicator box to be used to set up log
  QRect GraphControls();         //returns the graph controls box, to be used to set up log  
  QString TimerValue();         //returns the current timer value, to be used for log
  ~DAQSystem();                              //destructor
  
  public slots:
  void        SignalMonitor();       //monitors FIFOs - controlled by timer
  void        PauseDisplay(int);       //pauses appropriate display (not acq.)
  void        ToggleAllGraphs(bool);   //toggles every display -LG
  void        SetChannelGain(int);     //sets DAQ gain for appropriate channel
  void        SetSpikeThreshold(int);  //sets threshold for spike detection
  void        SetSpikeBlanking(int);   //ses blanking between spike detections
  void        SetSpikePolarity(int);   //change spike detection polarity
  void        ToggleRecordData(bool);  //toggles writing/not writing data - move to MainWidget
  void        QuitDAQSystem();         //writes data to file and quits
  void        ShowControls(int index);  //displays controls for channel denoted by index -LG
  void        HideControls(GraphParams& params); //hides controls for channel denoted by index -LG

  void        WhatsThis();           //Switches on the "What's This?" function
  void        ChangePage(int);
  void        ChangeChannels(int);
  void        SaveLog();                //writes the contents of the Log window into a text file
  void        InsertTime();            //Inserts the timer value into the log
 protected:
  void        InitGraphs(GraphParams&,SignalParams&); //initiates graphs
  void        SignalPlot(int,scan_index_t);     //plots FIFO data
  void        EraseGraph(int, int);    //erases the graph if it is not currently shown -LG
  void        GraphAxesCreate(GraphParams&,SignalParams&); //create axes
  void        ComputeYAxisTicks(SignalParams&,float *); //ticks
  void        CreateGraph_pm(QPixmap *,int,int,bool,int); //draws pm
  void        OpenOutputFile(); //move to MainWidget with f_out_pointer parameter
  void        SignalsToMappers(int which_signal); //connects signals to mappers
  void        PaletteSetup();
  void        WhatsThisSetup();            
  void        LogSetup();

 private:
  QWidget       *main_parent; //main_parent used throughout GUIs to access
                                  //widgets or functions (such as statusBar) 
                                  //of parent class

  int         PointsPerScreen;
  int         pixmapPerScreen;
  int         replotInterval;   //**** must be an integer or won't work
          

  int         ad_fifo_ptr;         //pointer to channel Real-Time FIFO
  int         local_scan_index;      //track scan index in daq
  int         vertical_blocks_per_screen;
  int         pixmap_width;
  int         pixmap_height;
 
  int         current_channel; //The channel whose properties are currently displayed - LG
  int         channels_on_page[MaxPages];    //Stores the number of channels on the page number corresponding to the index - LG
  int         current_page;                 //Page that is currently displayed

  bool        all_graphs_paused;  
  GraphParams    graph_params[NUM_AD_CHANNELS_TO_USE];

  ofstream f_out;                       //output file for ascii
  int f_out_pointer;                    //output file for binary

  //volatile SharedMemStruct *sh_mem; //compile fails w/ volatile
  SharedMemStruct *sh_mem; //define mbuff shared memory 

  QColor            graphColor;    //color used for graph background
  QPalette          GreenTextPalette;
  QPalette          RedTextPalette;
  QPalette          BlueTextPalette;
  QPalette          LightGrayTextPalette;
  QPalette          ScrollbarPalette; //palette used for scrollbars
  QPalette          RecordDataTextPalette;
  QPalette          RecordDataButtonOnPalette;
  QPalette          RecordDataButtonOffPalette;

  GraphingOptions   *graph_options;        //various graph properties such as ascii or binary data output, number of channels, etc    
  QTimer            *timer;                //timer for signalling ECGMonitor
  QPixmap           *graph_pm[MaxPixmapPerScreen]; //pixmap for graphs

  QSignalMapper     *gain_mapper;     //maps gain pulldowns to proper channel
  QSignalMapper     *threshold_mapper;//maps threshold scroller to prop chan
  QSignalMapper     *blanking_mapper; //maps blanking scroller to prop chan
  QSignalMapper     *polarity_mapper; //maps polarity  button to prop chan - LG
  QSignalMapper     *pause_mapper;    //maps pause button to prop chan

  QVBox             *top_box;         //holds the controls at the top, such as the timer and recording button - LG
  QHBox             *timer_group;             //Places timer label and timer lcd next to each other -LG
  QLabel            *timer_label;             //label for spike_number lcd
  QLabel            *timer_lcd;               //spike_number_lcd 

  bool               output_ascii;      //ascii (1) or binary (0) output

  QPushButton       *record_data_button;

  bool               record_data;
  bool               output_file_opened;

  QString            f_ascii; 
  QString            f_binary;
  QComboBox         *channel_pulldown;         //Select which channel's controls are displayed -LG
  QComboBox         *page_pulldown;           //Select which page to display

  QMultiLineEdit    *log;                    
  QPushButton       *log_save;                //Save button for log -LG
  QPushButton       *log_timer;               //Inserts a timestamp into the log -LG
};

/************************ MainWidget for main.cpp **********************/
class MainWidget : public QWidget
{

  Q_OBJECT
 public:
  MainWidget( SharedMemStruct *sh_mem, QWidget *parent=0, const char *name=0);
  QTimer          *timer;

  public slots:
    void        Copyright();             //Displays copyright information
  void          Help();              //Displays the help menu

 private:
  QPushButton       *quit_now;
  DAQSystem         *daq_system;
  QMenuBar          *menuBar;
  QPopupMenu        *helpMenu;
  QTextBrowser      *daq_help;
  QLabel            *daq_system_label; 
  GraphingOptions    options; //options initially set by configuration window -LG

  void              NameOutputFile();
  void              MenuBarSetup();
  void              CallConfigurationWindow();

};


#endif //DAQSystem_H
