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

#include "daq_system.h"

DAQSystem::DAQSystem(GraphingOptions &options, SharedMemStruct *sh_mem_param, QWidget *parent, const char *name)
  : QFrame(parent, name), main_parent(parent) {
  //main_parent (=parent) used throughout GUIs to access widgets 
  //or functions (such as statusBar) of parent class

  initMetaObject();  // initialize meta object

  //Shared Memory communicates with RT process
  sh_mem=sh_mem_param;

  //color palettes
  PaletteSetup();

  //Graphing options from MainWidget
  graph_options = &options;
  output_ascii=graph_options->use_output_ascii;
  sh_mem->num_ai_chan_in_use = graph_options->tot_num_channels;

  //Setting up the number of channels on each page - LG
  for(int iii=0; iii<graph_options->tot_pages; iii++)
    {
      if(graph_options->tot_num_channels%MaxChanPerPage && iii==graph_options->tot_pages-1) //last page and there will be empty graphs -LG
	{
	  channels_on_page[iii] = graph_options->tot_num_channels%MaxChanPerPage;
	}
      else channels_on_page[iii] = MaxChanPerPage;

    }
  current_page=0; //page initially displayed -LG 

  //general window characteristics
  setFrameStyle( QFrame::WinPanel | QFrame::Raised );
  setBackgroundColor(darkGray);

  //create and draw graph pixmaps - may need to change geometry to fit more channels

  int replotsPerSec;       //times per sec that ECG graph is refreshed
  switch (graph_options->sec_to_display) {   
  case 5:
    replotsPerSec=20;     
    break;
  case 10:
    replotsPerSec=20;     
    break;
  case 20:
    replotsPerSec=25;     
    break;
  case 25:
    replotsPerSec=20;     
    break;
  }

  replotInterval =   //**** must be an integer or won't work
    RT_HZ/replotsPerSec; //# of scans between ECG graph refreshes

  PointsPerScreen = graph_options->sec_to_display*RT_HZ;
  pixmapPerScreen = replotsPerSec*graph_options->sec_to_display;

  pixmap_width=GraphWidthInPixels/pixmapPerScreen;
  pixmap_height=GraphHeightInPixels;
  int y_btwn_graphs=ButtonHeight;

  all_graphs_paused = FALSE;

  top_box = new QVBox(this, "top_box"); //Box to hold controls at the top of the screen - LG 
  top_box->setGeometry(QRect(GraphXPosition + 3*BtwnButtons+ (pixmapPerScreen*pixmap_width), YBorder-SpaceAboveGraph-ButtonHeight,
  BoxWidth,8*ButtonHeight)); 
  top_box->setBackgroundColor(darkGray);
  top_box->setSpacing(3);
  vertical_blocks_per_screen=SuggestedVerticalBlocksPerScreen;
  while (pixmapPerScreen%vertical_blocks_per_screen) 
    //increment until pixmapPerScreen/vertical_blocks_per_screen is an integer
    vertical_blocks_per_screen++;  
  //cout << vertical_blocks_per_screen << endl;
  
  for (int jjj=0; jjj<pixmapPerScreen; jjj++) { 
    graph_pm[jjj] = new QPixmap();
    if(graph_options->tot_pages ==1)
      {
	if ((graph_options->num_channels)>2) 
	  pixmap_height=(MainWidgetHeight-2*YBorder)/(graph_options->num_channels)- y_btwn_graphs; //geometry does not need to be changed if <2
      }
    else pixmap_height=(MainWidgetHeight-2*YBorder)/(MaxChanPerPage)- y_btwn_graphs;
    bool draw_vertical_grid=FALSE;
    if (!(jjj%(pixmapPerScreen/vertical_blocks_per_screen)))
      draw_vertical_grid=TRUE;
    CreateGraph_pm(graph_pm[jjj],pixmap_width,pixmap_height,
		   draw_vertical_grid,HorizontalBlocksPerScreen);
  }

  // *********** Initialize datafile and recording
  output_file_opened=FALSE;
  record_data=FALSE; //TRUE=record data; initially TRUE to prevent 
                    //accidental non-recording


  //signal and graph initializations
  local_scan_index=0;   //synchronize local_scan_index
  for(int kk=0; kk < graph_options->num_channels; kk++) {

    graph_params[kk].page_number = kk/MaxChanPerPage;
    graph_params[kk].which_graph=kk%MaxChanPerPage;
    graph_params[kk].which_signal=kk; 

    if(graph_params[kk].page_number==current_page) graph_params[kk].display_graph=TRUE;
    else graph_params[kk].display_graph = FALSE;
 
    int signal = kk;
    sh_mem->signal_params[signal].spike_threshold=SCAN_UNITS;
    sh_mem->signal_params[signal].spike_blanking=InitSpikeBlanking;
    sh_mem->signal_params[signal].spike_index=0;
    sh_mem->signal_params[signal].channel_gain = INITIAL_CHANNEL_GAIN;
    sh_mem->ai_chan[signal]=       //send gain to the board via COMEDI
      CR_PACK(signal,INITIAL_CHANNEL_GAIN,AREF_GROUND);
 

     //set up properties for each graph

    QString graph_label_text;
    graph_label_text.sprintf("%d",kk);
    graph_params[kk].label="CH "+graph_label_text; 
    
    graph_params[kk].max_pixel=SCAN_UNITS;
    //graph_params[kk].x_position = GraphWidthInPixels;
    graph_params[kk].x_position=GraphXPosition;
    if (graph_params[kk].which_graph==0) graph_params[kk].y_position=YBorder-SpaceAboveGraph-ButtonHeight;
    else 
      {
	int previous_graph=graph_params[kk-1].which_graph;
	graph_params[kk].y_position=
	   graph_params[previous_graph].y_position+pixmap_height+y_btwn_graphs;
      }
   graph_params[kk].width=pixmap_width;
    graph_params[kk].height=pixmap_height;      
    
    for (int jjj=0; jjj<PointsPerScreen; jjj++)
      for (int iii=0; iii<(graph_options->num_channels); iii++)
	graph_params[iii].data[jjj]=0; //init graph data
    
    
    
    InitGraphs(graph_params[kk],sh_mem->signal_params[signal]);
    
    //**** connect signals to mappers
    SignalsToMappers(kk);
    //**** end of connect signals to mappers
  }


  //timer

  timer_group = new QHBox(top_box, "Timer Group", 0, TRUE);
  timer_group->setMaximumWidth(LabelWidth); 
  timer_group->setBackgroundColor(darkGray);  
  timer_group->setSpacing(0);
  timer_group->setMargin(0);
  top_box->setStretchFactor(timer_group, 3);
  timer_label = new QLabel( timer_group ); //timer label
  timer_label->setText( "time" );
  timer_label->setFixedSize(35, ButtonHeight);
  timer_label->
    setFrameStyle( QFrame::WinPanel | QFrame::Sunken );
  timer_label->setBackgroundColor(white);
  timer_label->setAlignment( AlignCenter );

  timer_lcd = new QLabel( timer_group ); //timer_lcd label
  timer_lcd->setFont( QFont( "Arial", 14, QFont::Bold ) );
  timer_lcd->setText( "0" );
  timer_lcd->setFixedSize(LabelWidth-35, ButtonHeight);
  timer_lcd->
    setFrameStyle( QFrame::WinPanel | QFrame::Sunken );
  timer_lcd->setAlignment( AlignCenter );
  
  //record data button (for all channels) 
 
  record_data_button = new QPushButton( "Not Recording", top_box );
  record_data_button->setFont(QFont( "Arial", 20, QFont::Bold ));
  record_data_button->setToggleButton(TRUE);
  record_data_button->setOn(record_data);
  record_data_button->setFixedSize(LabelWidth, 1.5*ButtonHeight);
  if (record_data) {
    record_data_button->setText("RECORDING");
    record_data_button->setPalette(RecordDataButtonOnPalette);
  }
  else record_data_button->setPalette(RecordDataButtonOffPalette);
  record_data_button->setFont( QFont( "Arial", 12, QFont::Bold ) );
  connect( record_data_button, SIGNAL(toggled(bool)), 
	   this, SLOT(ToggleRecordData(bool)));

  //space below record button - may need to be removed if a layout manager is used -LG 
  QVBox *space_box = new QVBox(top_box, "channel_space_box");
  space_box->setFixedSize(LabelWidth, 1); 
  space_box->setBackgroundColor(darkGray);

  QLabel *page_label = new QLabel(top_box);
  page_label->setText("Page Navigation");
  page_label->setPalette( RedTextPalette );
  page_label->setFont( QFont( "Arial", 12, QFont::Bold ) );
  page_label->
    setFrameStyle( QFrame::WinPanel | QFrame::Sunken );
  page_label->setAlignment( AlignCenter );
  page_label->setMaximumSize(LabelWidth, ButtonHeight);
  page_pulldown = new QComboBox(FALSE, top_box, "page_pulldown" );
  connect(page_pulldown, SIGNAL(activated(int)), this, SLOT(ChangePage(int)));

  //space below page pulldown - maybe need to be removed if a layout manager is used -LG 
  QVBox *space_box2 = new QVBox(top_box, "page_space_box");
  space_box2->setFixedSize(LabelWidth, 1); 
  space_box2->setBackgroundColor(darkGray);

 //Pull down menu to select which channel's properties are to be modified -LG

  QLabel *channel_label = new QLabel(top_box);
  channel_label->setText("Channel Selector");
  channel_label->setPalette( RedTextPalette );
  channel_label->setFont( QFont( "Arial", 12, QFont::Bold ) );
  channel_label->
    setFrameStyle( QFrame::WinPanel | QFrame::Sunken );
  channel_label->setAlignment( AlignCenter );
  channel_label->setMaximumSize(LabelWidth, ButtonHeight);
  channel_pulldown = new QComboBox(FALSE, top_box,   "channel_pulldown_box" );
  QString ch_item;

  for (int fff=0; fff<channels_on_page[current_page]; fff++)
    {
      ch_item.sprintf("CH %d", current_page*MaxChanPerPage+fff);
      channel_pulldown->insertItem(ch_item, fff);
      channel_pulldown->setCurrentItem(0);
    }
  
  channel_pulldown->setBackgroundColor("white");
  channel_pulldown->setMaximumSize(LabelWidth, ButtonHeight); 

  current_channel = 0; //Initially, the channel 0 controls will be displayed
  connect(channel_pulldown, SIGNAL(activated( int)), this, SLOT(ChangeChannels( int)));

  int first_ch;
  int last_ch;
  QString page_item;

  for (int fff=0; fff<graph_options->tot_pages; fff++)
    {
      first_ch = fff*MaxChanPerPage;
      last_ch = fff*MaxChanPerPage+channels_on_page[fff]-1;
      page_item.sprintf("CH %d - CH %d", first_ch, last_ch); //Need to subtract one because channel are numbered starting at zero - LG
      page_pulldown->insertItem(page_item, fff);
      page_pulldown->setCurrentItem(0);
    }
  page_pulldown->setFixedSize(LabelWidth, ButtonHeight);


  /******* initial clearing of FIFO buffers ********/
  fd_set  rfds;
  int ret_val=1;
  struct timeval tv;
  FD_ZERO(&rfds);
  FD_SET (ad_fifo_filedes, &rfds);  //ad_fifo_filedes initialized in main.cpp

  tv.tv_sec=0;
  tv.tv_usec=1;

  int init_num_scanned=0;
  static FIFO_struct ai_fifo_struct;

  while ((ret_val=select(FD_SETSIZE, &rfds, NULL, NULL, &tv))) {
    read ( ad_fifo_filedes, &ai_fifo_struct, sizeof(ai_fifo_struct));
    init_num_scanned++;
  }
  debug ("\ninitial FIFO clear: %d scanned",init_num_scanned);
  /******* end of initial clearing of buffer ********/
  
  LogSetup();

  WhatsThisSetup();
  
}

void DAQSystem::ChangeChannels(int id)
{
  //id is the graph number - LG
  int signal = current_page*MaxChanPerPage+id; //signal corresponding to graph number
  ShowControls(signal);
}


void DAQSystem::ChangePage(int new_page)
{

  if(current_page!=new_page)
    {
      int first_ch = new_page*MaxChanPerPage;

      //hiding and displaying the appropriate graphs
      for(int kk=0; kk<graph_options->tot_num_channels; kk++)
	{
	  if(graph_params[kk].page_number!=new_page)
	    {
	      graph_params[kk].monitor_label->hide();
	      for (int jj=0; jj<NumYAxisTicks; jj++)
		graph_params[kk].y_axis_tick[jj]->hide();
	      graph_params[kk].display_graph=FALSE;
	    }
	  else
	    {
	      graph_params[kk].monitor_label->show();
	      for (int jj=0; jj<NumYAxisTicks; jj++)
		graph_params[kk].y_axis_tick[jj]->show();
	      graph_params[kk].display_graph=TRUE;
	    }
	}

      //setting up channel pulldown menu
      channel_pulldown->clear();
      
      QString ch_item;
      for (int fff=0; fff<channels_on_page[new_page]; fff++)
	{
	  ch_item.sprintf("CH %d", new_page*MaxChanPerPage+fff);
	  channel_pulldown->insertItem(ch_item, fff);
	  channel_pulldown->setCurrentItem(0);
	}
 
      ShowControls(first_ch);

      current_page = new_page;
    }
}

void DAQSystem::SignalMonitor() {        //for use with timer 
  
  static FIFO_struct ai_fifo_struct;
  scan_index_t graph_plot_index=0;

  fd_set  rfds;    //these variables are for reading FIFO until it's empty
  int ret_val=1;
  struct timeval tv;
  tv.tv_sec=0;
  tv.tv_usec=1;

  FD_ZERO(&rfds);
  FD_SET( ad_fifo_filedes, &rfds);

  //while loop reads sh_mem->signal_params[0].channel_num fifo until empty
  //if after tv, there is no element there, ret_val=0
  while ((ret_val=select(FD_SETSIZE, &rfds, NULL, NULL, &tv))) {

    float scan_time = local_scan_index/(float)(RT_HZ);
    //writes to file
    if (record_data) {
      if (output_ascii) f_out<<(local_scan_index/(float)(RT_HZ))<<" ";//ascii
      else write(f_out_pointer, &scan_time, sizeof(float)); //binary
    }

    //read entire ai_fifo_struct off of FIFO
    read (ad_fifo_filedes, &ai_fifo_struct, sizeof(ai_fifo_struct));
    graph_plot_index=((local_scan_index)%PointsPerScreen); 
    
    for (int kk=0; kk<(graph_options->num_channels); kk++) { //store data
      //int signal  = graph_params[kk].which_signal; 
      if (graph_params[kk].display_graph)
	graph_params[kk].data[graph_plot_index]=ai_fifo_struct.data[kk];
      else graph_params[kk].data[graph_plot_index] = 0;
    }

    for (int kk=0; kk<(graph_options->num_channels); kk++) { //store data
      if (record_data) { //convert from scans to volts
	int channel_gain = sh_mem->signal_params[kk].channel_gain;
	float volts= ai_fifo_struct.data[kk];//graph_params[kk].data[graph_plot_index];
	volts /= SCAN_UNITS;
	volts *= ( hardwareSpecificStruct.gainSettings[1][channel_gain] -
		   hardwareSpecificStruct.gainSettings[0][channel_gain]);
	volts += hardwareSpecificStruct.gainSettings[0][channel_gain];
	if (record_data) {
	  if (output_ascii) f_out << volts << " ";          //ascii
	  else write(f_out_pointer, &volts, sizeof(float)); //binary
	}
      }
   }     /*************************/
    if (record_data&&output_ascii) f_out << endl; //eol for ascii


    //plot ECG every replotInterval scans
    if (!((graph_plot_index)%replotInterval)) {
      QString time_text;
      time_text.sprintf("%.0f",scan_time);
      //update signal plots
      //execute this code only for the daq_system that's currently displayed
      for (int kk=0; kk<(graph_options->num_channels); kk++) {
	if(graph_params[kk].display_graph &&!graph_params[kk].pause_display)
	  SignalPlot(kk,graph_plot_index);  
	else if(graph_params[kk].which_graph >=channels_on_page[current_page])
	  //the hidden graph is not covered up by another graph - LG
	  EraseGraph(graph_params[kk].which_graph, graph_plot_index);
      }
      timer_lcd->setText(time_text);
    }
    //spike indication
    for (int kk=0; kk<(graph_options->num_channels); kk++) {
      int signal  = kk; //graph_params[kk].which_signal;
	if (sh_mem->signal_params[signal].found_spike) {
	  float rr_interval=
	    1000.0*(sh_mem->signal_params[signal].found_spike-
	            sh_mem->signal_params[signal].previous_spike)/(float)(RT_HZ);
	  //cout <<"rr_interval="<<rr_interval
	  //     <<"found_spike="<< sh_mem->signal_params[kk].found_spike
	  //     <<"previous_spike="<< sh_mem->signal_params[kk].previous_spike
	  //     <<endl;
	  sh_mem->signal_params[signal].previous_spike=//reset previous to current
	    sh_mem->signal_params[signal].found_spike;
	  sh_mem->signal_params[signal].found_spike=0; //clear current found_spike

	  QString rr_interval_text;
	  rr_interval_text.sprintf("%.0f",rr_interval);
	  graph_params[kk].spike_indicator->setText(rr_interval_text); //RR
	  //graph_params[kk].turn_off_spike_indicator = 
	  //  local_scan_index+SpikeIndicatorDuration;
	  /****************/
	}
	//if (local_scan_index == graph_params[kk].turn_off_spike_indicator) 
	//  graph_params[kk].spike_indicator->setText( " " );//clear indicator 
    }
    local_scan_index++;
  }

  //cout << "At end of FIFO read, local_scan_index= " << local_scan_index
    //<<endl;

}

void DAQSystem::SignalPlot(int signal_num, scan_index_t graph_plot_index) {

  //SignalPlot uses QPainter and bitBlt to draw the new data onto
  //the current pixmap on the "monitor screen". The screen is broken
  //up into multiple pixmaps to make it so that the entire trace does
  //not need to be drawn every time, only the small portion of the 
  //trace within the current_pm.

  int current_pm = graph_plot_index/replotInterval;
  //if ((!signal_num)&&(graph_plot_index<40))
  //  cout << "current_pm=" << current_pm << "  graph_plot_index=" 
  //	 << graph_plot_index <<endl;
  int signal  = graph_params[signal_num].which_signal;
  QPainter    pb;
  QPixmap     pm = QPixmap (*graph_pm[current_pm]);
  QPen        pen;

  int y_axis_max_pixel=SCAN_UNITS;
  pb.begin (&pm);
  pb.setWindow (0, 0, replotInterval, y_axis_max_pixel);

  //draw signal trace
  pen.setColor (red);
  pen.setWidth (2);
  pen.setStyle (SolidLine);
  pb.setPen (pen);

  //array of points to plot in the current pixmap
  QPointArray new_points_to_plot(replotInterval+1); 

  /** 
   ** NOTE: new_points_to_plot has replotInterval+1 points so that the 
   ** polylines from consecutive pixmaps are connected. At first glance,
   ** it may seem that the same point is being drawn twice, but it's just
   ** that in one run through the last point is the endpoint of the polyline
   ** and the next run through the same point is the beginpoint of the polyline
   ** ... I didn't really check to see if this could cause problems with
   ** SEGFAULT in case the last point is getting ahead of the FIFO data, though
   **/ 

  //data

  for (int jj=0; jj<=replotInterval; jj++) {
    int this_plot_x_index=(jj+graph_plot_index-replotInterval)%PointsPerScreen;
    if (this_plot_x_index<0) this_plot_x_index+=PointsPerScreen;
    //cout <<"X Index:" <<this_plot_x_index <<endl;
    new_points_to_plot.setPoint(jj,jj,
	y_axis_max_pixel-graph_params[signal_num].data[this_plot_x_index]);
    //if ((!signal_num)&&(graph_plot_index<40))
    //cout << graph_plot_index << " " << this_plot_x_index << endl;
  }
  pb.drawPolyline(new_points_to_plot);		   //draw the lines

  //draw line at spike_threshold
  if(graph_options->show_spike)
    {
      pen.setColor (lightGray);
      pen.setWidth (2);
      pen.setStyle (DotLine);
      pb.setPen (pen);
      pb.drawLine (0,y_axis_max_pixel-
	       sh_mem->signal_params[signal].spike_threshold,
		   2*replotInterval,
		   y_axis_max_pixel-
		   sh_mem->signal_params[signal].spike_threshold);
    
      //draw thick line the length of spike_blanking ... only in relevant pixmaps
      if ( (sh_mem->signal_params[signal].spike_blanking)>graph_plot_index ) {
	pen.setColor (gray);
	pen.setWidth (10);
	pen.setStyle (SolidLine);  //pen.setStyle (DotLine);
	pb.setPen (pen);
	pb.drawLine (0,100,sh_mem->signal_params[signal].spike_blanking,100);
      }
    }
  pb.end();

  bitBlt(this, graph_params[signal_num].x_position+(current_pm*pm.width()),
	  graph_params[signal_num].y_position, &pm);

  //draw the gap that immediately follows the points 
  int gap_pm_index = (current_pm+1)%pixmapPerScreen; 
  QPixmap  gap_pm = QPixmap (*graph_pm[gap_pm_index]);
  bitBlt(this, graph_params[signal_num].x_position+
	                                      (gap_pm_index*gap_pm.width()),
	  graph_params[signal_num].y_position, &gap_pm);
}

void DAQSystem::EraseGraph(int signal_num, int graph_plot_index)
{
  int current_pm = graph_plot_index/replotInterval;
  QPainter    pb;
  QPixmap     pm = QPixmap (*graph_pm[current_pm]);
  pm.fill(darkGray);
  pb.begin(&pm);
  pb.end();

  bitBlt(this, graph_params[signal_num].x_position+(current_pm*pm.width()),
	 graph_params[signal_num].y_position, &pm);

}


void DAQSystem::PauseDisplay(int which_graph) {

  QPixmap pixmap(LabelWidth, ButtonHeight);
  pixmap.fill(white);
  QPainter *p = new QPainter(&pixmap);
  QPen pen;
  graph_params[which_graph].pause_display = 
      graph_params[which_graph].pause_button->isOn();
  if (graph_params[which_graph].pause_display)
    {
      pen.setColor(blue);
      pen.setWidth(5);
      pen.setStyle(SolidLine);
      p->setPen(pen);
      p->setBrush(blue);
      p->drawText(5,15, "Paused",6 );
      pen.setColor(lightGray);
      p->setPen(pen);
      p->drawText(55, 15, "Plotting",9 );
      graph_params[which_graph].pause_button_label->setPixmap(pixmap);
     }

  else 
    {
      pen.setColor(lightGray);
      pen.setWidth(5);
      pen.setStyle(SolidLine);
      p->setPen(pen);
      p->setBrush(lightGray);
      p->drawText(5,15, "Paused",6 );
      pen.setColor(blue);
      p->setPen(pen);
      p->drawText(55, 15, "Plotting",9 );
      graph_params[which_graph].pause_button_label->setPixmap(pixmap);

    }
  p->end();
}

void DAQSystem::ToggleAllGraphs(bool on) {

      for (int iii=0; iii<graph_options->num_channels; iii++)
	graph_params[iii].pause_button->setOn(on);
      all_graphs_paused = on;

  //The appropriate signal will automatically be emitted by each pause button to toggle the corresponding graph - LG
}
void DAQSystem::SetChannelGain(int signal) {

  int channel_gain=graph_params[signal].gain_pulldown->currentItem();
  //write channel gain via COMEDI's CR_PACK
  sh_mem->signal_params[signal].channel_gain=channel_gain;
  sh_mem->ai_chan[signal]=
    CR_PACK(signal,channel_gain,AREF_GROUND);

  //reset y-axis values 
  float y_val[NumYAxisTicks];
  ComputeYAxisTicks(sh_mem->signal_params[signal],y_val);
  for (int jj=0; jj<NumYAxisTicks; jj++) {
    QString y_axis_value;
    y_axis_value.sprintf("%.2f",y_val[jj]);

    graph_params[signal].y_axis_tick[jj]->setText(y_axis_value+" V");
  }
}

void DAQSystem::SetSpikeThreshold(int which_graph) { 

  int signal  = graph_params[which_graph].which_signal;
  sh_mem->signal_params[signal].spike_threshold=
      graph_params[which_graph].threshold_scroller->value();
}

void DAQSystem::SetSpikeBlanking(int which_graph) { 
  int signal  = graph_params[which_graph].which_signal;
  sh_mem->signal_params[signal].spike_blanking=
      graph_params[which_graph].blanking_scroller->value();

  cout << "spike_blanking=" <<sh_mem->signal_params[signal].spike_blanking<<endl;
}

void DAQSystem::SetSpikePolarity(int which_graph) {  

  int signal  = graph_params[which_graph].which_signal;
  
  int spike_polarity;
  spike_polarity=graph_params[which_graph].plus_button->isOn();
  sh_mem->signal_params[signal].spike_polarity=spike_polarity;

}

/*void DAQSystem::ChangeOutputMode(int which_type) {

  output_ascii= (!output_ascii); //toggle output_ascii
  ascii_button->setChecked(output_ascii);
  binary_button->setChecked(!(output_ascii));
  which_type=0; //purpose of this line is to elim. warning: which_type not used
  }*/

void DAQSystem::ToggleRecordData(bool toggle_value) {

  record_data=toggle_value;
  //record_data_button->setEnabled(record_data);
  if (record_data) {
    if (!output_file_opened) { //if this is first time enabled, open file
      OpenOutputFile(); //open output file
      output_file_opened=TRUE;
    }
    record_data_button->setText( "Recording" );
    record_data_button->setPalette(RecordDataButtonOnPalette);
  }
  else {
    record_data_button->setText( "Not Recording" );
    record_data_button->setPalette(RecordDataButtonOffPalette);
  }
  for (int kk=0; kk<(graph_options->num_channels); kk++)
    graph_params[kk].monitor_label->setEnabled(record_data);

  //once we start recording, no longer allow user to change data type
  //ascii_button->setEnabled(FALSE);
  //binary_button->setEnabled(FALSE);

}

void DAQSystem::SignalsToMappers(int which_graph)
{

  //**** connect mappers to slots

  //SignalMappers to connect widgets to actions for corresponding channel
  if(which_graph==0) //only initialize signal mappers once -LG
    {
      gain_mapper=new QSignalMapper(this);
      threshold_mapper=new QSignalMapper(this);
      blanking_mapper=new QSignalMapper(this);
      polarity_mapper=new QSignalMapper(this);
      pause_mapper=new QSignalMapper(this);
      if(graph_options->show_spike) //Spike Indicator is displayed
	{
	  connect (polarity_mapper, SIGNAL(mapped(int)),
		   this, SLOT(SetSpikePolarity(int)));
	  connect (blanking_mapper, SIGNAL(mapped(int)),
		   this, SLOT(SetSpikeBlanking(int)));
	  connect (threshold_mapper, SIGNAL(mapped(int)),
		   this, SLOT(SetSpikeThreshold(int)));
	}
      
      connect (gain_mapper, SIGNAL(mapped(int)),
	       this, SLOT(SetChannelGain(int)));
      connect (pause_mapper, SIGNAL(mapped(int)),
	       this, SLOT(PauseDisplay(int)));
    }
  
  pause_mapper->setMapping(graph_params[which_graph].pause_button, which_graph);
  connect (graph_params[which_graph].pause_button, SIGNAL(toggled(bool)),
	   pause_mapper, SLOT(map()));
  
  gain_mapper->setMapping(graph_params[which_graph].gain_pulldown, which_graph);
  connect (graph_params[which_graph].gain_pulldown, SIGNAL(activated(int)),
	   gain_mapper, SLOT(map()));
  
  if(graph_options->show_spike) //If spike indicator is displayed -LG
    {
      threshold_mapper->setMapping(graph_params[which_graph].threshold_scroller, which_graph);
      connect (graph_params[which_graph].threshold_scroller, SIGNAL(valueChanged(int)), threshold_mapper, SLOT(map()));
      
      blanking_mapper->setMapping(graph_params[which_graph].blanking_scroller, which_graph);
      connect (graph_params[which_graph].blanking_scroller, SIGNAL(valueChanged(int)),
	       blanking_mapper, SLOT(map()));
      
      polarity_mapper->setMapping(graph_params[which_graph].polarity_group, which_graph);
      connect (graph_params[which_graph].polarity_group, SIGNAL(clicked(int)),
	       polarity_mapper, SLOT(map()));
    }
  //**** end of connect mappers to slots
}
void DAQSystem::ShowControls(int index)
{
  if ( index == current_channel)
    return;
  //execute code only if a different channel has been selected -LG
  
  HideControls(graph_params[current_channel]); 
  
  graph_params[index].graph_controls->show();
  graph_params[index].pause_button->show();            //button to pause plotting
  graph_params[index].pause_group->show();
  graph_params[index].pause_all->show();
  graph_params[index].pause_all->setOn(all_graphs_paused);
  graph_params[index].pause_button_label->show();      //label for paused/not paused
  graph_params[index].gain_pulldown->show();           //pulldown box for channel_gain
  if(graph_options->show_spike) {  
    graph_params[index].spike_frame->show();
    graph_params[index].spike_box->show();
    graph_params[index].polarity_group->show();         //toggles spike polarity
    graph_params[index].polarity_button_label->show();   //label for button
    graph_params[index].plus_button->show();
    graph_params[index].minus_button->show();
    graph_params[index].scroller_label->show();          //label for threshold
    graph_params[index].threshold_scroller->show();     //scroller for spike_threshold
    graph_params[index].blanking_scroller->show();       //scroller for spike_blanking
    graph_params[index].blanking_scroller_label->show();
    graph_params[index].spike_indicator->show();         // ** indicates a spike
    graph_params[index].spike_indicator_label->show();   //labels indicator
  }

  current_channel = index;
  
}

void DAQSystem::HideControls(GraphParams& params)
{
  params.graph_controls->hide();
  params.pause_button->hide();            //button to pause plotting
  params.pause_all->hide();
  params.pause_group->hide();
  params.pause_button_label->hide();      //label for paused/not paused
  params.gain_pulldown->hide();           //pulldown box for channel_gain
  if(graph_options->show_spike)
    {
      params.spike_box->hide();
      params.spike_frame->hide();
      params.polarity_group->hide();         //toggles spike polarity
      params.polarity_button_label->hide();   //label for button
      params.plus_button->hide();
      params.minus_button->hide();
      params.scroller_label->hide();          //label for threshold
      params.threshold_scroller->hide();      //scroller for spike_threshold
      params.blanking_scroller->hide();       //scroller for spike_blanking
      params.blanking_scroller_label->hide(); //label for spike blaking
      params.spike_indicator->hide();         // ** indicates a spike
      params.spike_indicator_label->hide();   //labels indicator
    }
  
}

//InitGraphs places buttons on right side. Makes them invisible.

void DAQSystem::InitGraphs( GraphParams& params, SignalParams& sh_mem_params ) {

  int previous_y = top_box->y();
  int previous_height=top_box->height()+BtwnButtons;                //height of previous object 
  int x_coordinate = params.x_position + (pixmapPerScreen*pixmap_width) +3*BtwnButtons;

  QVBox *space_box[4]; //empty widget for spacing between controls - LG

  params.monitor_label = new QLabel( this );
  params.monitor_label->setText(params.label); //Channel label
  params.monitor_label->setFont( QFont( "Times", 18, QFont::Bold ) );
  params.monitor_label->
    setGeometry( x_coordinate-3*BtwnButtons-50,
		params.y_position,
		50,ButtonHeight);
  params.monitor_label->setAlignment(AlignCenter);
  params.monitor_label->setPalette(RecordDataTextPalette);
  params.monitor_label->setEnabled(record_data);
  if(!params.display_graph) params.monitor_label->hide();

  params.graph_controls = new QVBox(this, "graph_controls");
  params.graph_controls->setGeometry( x_coordinate,
  previous_y+previous_height,
  BoxWidth,4*ButtonHeight-BtwnButtons);
  
  params.graph_controls->setBackgroundColor(darkGray);
  params.graph_controls->setSpacing(0);
  params.graph_controls->setMargin(0);

  params.gain_pulldown = new QComboBox(FALSE, params.graph_controls, 
				  "gain_pulldown_box" );
  params.gain_pulldown->insertStrList( 
		  (const char **)hardwareSpecificStruct.gainNames, 
		  NUM_GAIN_PARAMETERS, 
		  -1 );
  params.gain_pulldown->setBackgroundColor(white);
  params.gain_pulldown->setCurrentItem(sh_mem_params.channel_gain);
  params.gain_pulldown->setFixedSize(LabelWidth, ButtonHeight);

  space_box[0]=new QVBox(params.graph_controls);
  space_box[0]->setFixedSize(LabelWidth, BtwnButtons);
  space_box[0]->setBackgroundColor(darkGray);

//PAUSE pushbutton for pausing ECG display
 
  params.pause_button_label = new QLabel( params.graph_controls ); //pause button label
  params.pause_button_label->setFixedSize(LabelWidth, ButtonHeight);
  params.pause_button_label->
    setFrameStyle( QFrame::WinPanel | QFrame::Sunken );
  params.pause_button_label->setBackgroundColor(white);
  params.pause_button_label->setAlignment( AlignCenter );
  params.pause_button_label->setPalette( RedTextPalette );
  params.pause_button_label->setFont(QFont("Arial", 12, QFont::Bold));

  params.pause_group = new QHBox(params.graph_controls, "pause_group", 0, TRUE) ; 
  params.pause_group->setFrameStyle( QFrame::WinPanel | QFrame::Sunken );
  params.pause_group->setFixedSize(LabelWidth, ButtonHeight+BtwnButtons); 
  params.pause_group->setBackgroundColor(darkGray);
  params.pause_group->setSpacing(0);

  params.pause_label = new QLabel( params.pause_group);
  params.pause_label->setText("Pause:");
  params.pause_label->setFixedSize(40, ButtonHeight);
  params.pause_label->
    setFrameStyle( QFrame::NoFrame );
  params.pause_label->setBackgroundColor(lightGray);
  params.pause_label->setAlignment( AlignCenter );
  params.pause_label->setFont(QFont("Arial", 12, QFont::Normal));

  QString channel_to_pause;
  channel_to_pause.sprintf("CH %d", params.which_signal);
  params.pause_button = new QPushButton( channel_to_pause, params.pause_group);
  params.pause_button->setToggleButton( TRUE );
  params.pause_button->setOn(FALSE);
  params.pause_display = FALSE;
  params.pause_button->setFixedSize(ButtonWidth-20, ButtonHeight);

  //PAUSE pushbutton for pausing all ECG displays -LG
  params.pause_all = new QPushButton( "All", params.pause_group);
  params.pause_all->setToggleButton( TRUE );
  params.pause_all->setOn(FALSE);
  params.pause_all->setFixedSize(ButtonWidth-20, ButtonHeight);
  PauseDisplay(params.which_signal); //since initially button is not pressed, label will be set correctly  
  connect(params.pause_all, SIGNAL(toggled(bool)), this, SLOT(ToggleAllGraphs(bool)));

  previous_height = params.graph_controls->height();
  previous_y =  params.graph_controls->y();

  if(graph_options->show_spike)
    {
      //Setting up group box for spike indicator properties
      params.spike_frame = new QGroupBox(1,Horizontal, "Spike Indicator", this, "Spike Indicator");  
      params.spike_frame->setGeometry(x_coordinate, BtwnButtons+ previous_y + previous_height, BoxWidth, 10*ButtonHeight-BtwnButtons);
      params.spike_frame->setBackgroundColor(darkGray);
      params.spike_frame->setMargin(0);
      
      params.spike_box=new QVBox(params.spike_frame);
      params.spike_box->setSpacing(2);
      params.spike_box->setBackgroundColor(darkGray);

      /*params.spike_box = new QGroupBox(7,Vertical, "Spike Indicator", this, "Spike Indicator");
      params.spike_box->setGeometry(x_coordinate, BtwnButtons+ previous_y + previous_height, BoxWidth, 9*ButtonHeight+2*BtwnButtons);
      params.spike_box->setBackgroundColor(darkGray);
      params.spike_box->setMargin(0);
      */
     //spike detection polarity

      sh_mem_params.spike_polarity=TRUE; //TRUE=Positive

      params.polarity_group = new QButtonGroup(3, Horizontal, params.spike_box, "polarity_group") ; 
      params.polarity_group->setFrameStyle( QFrame::NoFrame );
      params.polarity_group->setFixedSize(LabelWidth, ButtonHeight+2*BtwnButtons); 
      params.polarity_group->setMargin(0);
      params.polarity_group->setBackgroundColor(darkGray);
      params.polarity_group->setExclusive(TRUE);
      params.polarity_group->setLineWidth(0);
      params.polarity_group->setMargin(0);
      params.polarity_group->setMidLineWidth(0);
      params.polarity_group->setAlignment(AlignHCenter);

      params.polarity_button_label = new QLabel( params.polarity_group);
      params.polarity_button_label->setText("Polarity:");
      //params.polarity_button_label->setFixedSize(LabelWidth-45, ButtonHeight);
      params.polarity_button_label->
	setFrameStyle( QFrame::NoFrame );
      params.polarity_button_label->setBackgroundColor(darkGray);
      params.polarity_button_label->setAlignment( AlignCenter );
      params.polarity_button_label->setFont(QFont("Arial", 12, QFont::Normal));

      params.plus_button = new QPushButton( "+", params.polarity_group);
      params.plus_button->setToggleButton( TRUE );
      params.plus_button->setOn(TRUE);
      params.plus_button->setFixedSize(22, ButtonHeight);
      
      params.minus_button = new QPushButton( "-", params.polarity_group);
      params.minus_button->setToggleButton( TRUE );
      params.minus_button->setOn(FALSE);
      params.minus_button->setFixedSize(22, ButtonHeight);

      space_box[1]=new QVBox(params.spike_box);
      space_box[1]->setFixedSize(LabelWidth, BtwnButtons);
      space_box[1]->setBackgroundColor(darkGray);

      params.scroller_label = new QLabel( params.spike_box ); //spike_threshold label
      params.scroller_label->setPalette( RedTextPalette );
      params.scroller_label->setFont( QFont( "Arial", 12, QFont::Bold ) );
      params.scroller_label->setText( "Threshold" );
      params.scroller_label->
	setFrameStyle( QFrame::WinPanel | QFrame::Sunken );
      params.scroller_label->setAlignment( AlignCenter );
      params.scroller_label->setMaximumSize(LabelWidth, ButtonHeight);
  
    //Spike threshold scrollbar
      params.threshold_scroller = 
	new QScrollBar( 0, SCAN_UNITS, //range
			1, 10,         //line/page steps
			SCAN_UNITS,    //initial v
			QScrollBar::Horizontal, //orientation
			params.spike_box, "scrollbar" );
      params.threshold_scroller->setMaximumSize(LabelWidth, ButtonHeight);

      space_box[2]=new QVBox(params.spike_box);
      space_box[2]->setFixedSize(LabelWidth, BtwnButtons);
      space_box[2]->setBackgroundColor(darkGray);

      params.blanking_scroller_label = new QLabel( params.spike_box ); //spike_threshold label
      params.blanking_scroller_label->setPalette( RedTextPalette );
      params.blanking_scroller_label->setFont( QFont( "Arial", 12, QFont::Bold ) );
      params.blanking_scroller_label->setText( "Blanking" );
      params.blanking_scroller_label->
	setFrameStyle( QFrame::WinPanel | QFrame::Sunken );
      params.blanking_scroller_label->setAlignment( AlignCenter );
      params.blanking_scroller_label->setMaximumSize(LabelWidth, ButtonHeight);
  //Spike blanking scrollbar
      params.blanking_scroller = 
	new QScrollBar( MinSpikeBlanking,MaxSpikeBlanking,
			1, 10,                  //line/page steps
			InitSpikeBlanking,      //initial
			QScrollBar::Horizontal, //orientation
			params.spike_box, "blanking scrollbar" );
      
      params.blanking_scroller->setMaximumSize(LabelWidth, ButtonHeight);

      space_box[3]=new QVBox(params.spike_box);
      space_box[3]->setFixedSize(LabelWidth, BtwnButtons);
      space_box[3]->setBackgroundColor(darkGray);

      //spike_indicator flashes when a spike is detected
      params.spike_indicator_label = new QLabel( params.spike_box ); //spike_indicator label
      params.spike_indicator_label->setText( "RR (ms)" );
      params.spike_indicator_label->
	setFrameStyle( QFrame::WinPanel | QFrame::Sunken );
      params.spike_indicator_label->setBackgroundColor(white);
      params.spike_indicator_label->setAlignment( AlignCenter );
      params.spike_indicator_label->setMaximumSize(LabelWidth, ButtonHeight);

      params.spike_indicator = new QLabel( params.spike_box ); //spike_indicator label
      params.spike_indicator->setFont( QFont( "Arial", 18, QFont::Bold ) );
      params.spike_indicator->setText( " " );
      params.spike_indicator->
	setFrameStyle( QFrame::WinPanel | QFrame::Sunken );
      params.spike_indicator->setAlignment( AlignCenter );
      params.spike_indicator->setMaximumSize(LabelWidth, ButtonHeight);

  previous_height = params.spike_box->height();
  previous_y =  params.spike_box->y();
    }

  GraphAxesCreate(params,sh_mem_params);  //create axes
  
  /*
  //draw the screens themselves
  for (int this_pm_index=0; this_pm_index<pixmapPerScreen; this_pm_index++) {
    QPixmap  this_pm = QPixmap (*graph_pm[this_pm_index]);
    bitBlt(this, params.x_position+(this_pm_index*this_pm.width()),
	   params.y_position, &this_pm);
  }
  */

  //Initially, controls for channel zero are shown -LG
  if(params.which_signal!=0)
    {
      HideControls(params);
    }
}

void DAQSystem::GraphAxesCreate(GraphParams& params, 
				SignalParams& sh_mem_params) {

  int jj;

  // ***** Y AXIS ***** //
  float y_val[NumYAxisTicks];
  ComputeYAxisTicks(sh_mem_params,y_val);

  for (jj=0; jj<NumYAxisTicks; jj++) {
    //set y_axis labels
    params.y_axis_tick[jj] = new QLabel( this );
    params.y_axis_tick[jj]->setFont( QFont( "Times", AxisFontSize, 
					    QFont::Bold ) );
    params.y_axis_tick[jj]->setBackgroundColor(darkGray);   
    params.y_axis_tick[jj]->
      setGeometry(params.x_position-AxisLabelWidth, 
		  params.y_position+(NumYAxisTicks-1-jj)*
		  graph_pm[0]->height()/(NumYAxisTicks-1)-(ButtonHeight/2),
		  AxisLabelWidth,.75*ButtonHeight);
    params.y_axis_tick[jj]->setAlignment( AlignCenter );
    QString y_axis_value;
    y_axis_value.sprintf("%.2f",y_val[jj]);
    params.y_axis_tick[jj]->setText(y_axis_value+" V");
    if(params.page_number!=current_page)
      params.y_axis_tick[jj]->hide();
  }

  //**** if statement using graph_params.which_graph here

  if( params.which_graph==channels_on_page[params.page_number]-1 ) //last graph on page
    {
      // ***** X AXIS time label ***** //
      char time_label_text[50];       //string for y axis min
      char time_divisions_text[50];   //string for y axis mid
      sprintf(time_divisions_text, "%.2f", 
	      (float)graph_options->sec_to_display/vertical_blocks_per_screen);
      strcat(time_divisions_text, " seconds/division");
      sprintf(time_label_text, " ");
      strcat(time_label_text, time_divisions_text);
      
      QLabel *x_axis_label = new QLabel( this ); 
      x_axis_label->setText(time_label_text);
      x_axis_label->setFont( QFont( "Times", 18, QFont::Bold ) );
      x_axis_label->
	setGeometry(params.x_position+(pixmapPerScreen*graph_pm[0]->width())/2-150,
		    params.y_position+graph_pm[0]->height(),300, ButtonHeight );
      x_axis_label->setBackgroundColor(darkGray);
      x_axis_label->setAlignment( AlignCenter );
    }
}

void DAQSystem::ComputeYAxisTicks(SignalParams& sh_mem_params,
				  float *y_val) {
  int jj;
  //if low gain<>0, then this is bipolar
  if (hardwareSpecificStruct.gainSettings[0][sh_mem_params.channel_gain]) {
    y_val[NumYAxisTicks/2]=0;
    for (jj=0; jj<(NumYAxisTicks/2); jj++) {
      y_val[jj]=hardwareSpecificStruct.gainSettings[0][sh_mem_params.channel_gain]-
	    jj*hardwareSpecificStruct.gainSettings[0][sh_mem_params.channel_gain]/
	    ((NumYAxisTicks-1)/2.0);
      y_val[NumYAxisTicks-jj-1] = -y_val[jj];
    }
  }
  else {  //if low gain=0, then this is unipolar
    for (jj=0; jj<(NumYAxisTicks); jj++) {
      y_val[jj]=
	jj*hardwareSpecificStruct.gainSettings[1][sh_mem_params.channel_gain]/
	(NumYAxisTicks-1);
    }
  }
}

void DAQSystem::CreateGraph_pm(QPixmap *graph_pm, int width, int height,
				     bool draw_vertical_grid,
				     int num_horizontal_grids) {
  QPainter    pb;
  QPen        pen;
  graph_pm->resize(width, height);
  graph_pm->fill(graphColor);  //background color
  pb.begin (graph_pm);
  int relative_window_width=1;
  pb.setWindow (0, 0, relative_window_width, SCAN_UNITS);

  pen.setColor(black);
  pen.setWidth(1);
  pen.setStyle(DotLine);
  pb.setPen (pen);

  //draw vertical gridline if indicated
  if (draw_vertical_grid) pb.drawLine(0,SCAN_UNITS,0,0);

  //draw horizontal gridlines
  for (int j=1; j<num_horizontal_grids; j++) {
    int y_grid_position= 
      (int)(((float)j/(float)num_horizontal_grids)*SCAN_UNITS);
    pb.drawLine(0,y_grid_position,relative_window_width,y_grid_position);
  }

  pen.setStyle(SolidLine); //reset pen to solid
  pb.setPen (pen);
  pb.end ();    //end QPainter
}



void DAQSystem::OpenOutputFile() { //open ascii OR binary output file

  if (output_ascii)   //open ascii output file
     f_out.open(graph_options->output_ascii,ios::out); 
  else {              //open binary output file
    cerr <<graph_options->output_binary <<endl;
  //create (O_CREAT) and open as write only (O_WRONLY)
  //S_IRUSR, S_IWUSR, S_IRGRP, and S_IROTH set permissions (see man open)
  if ((f_out_pointer = open(graph_options->output_binary, O_WRONLY | O_CREAT,
			    S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) < 0) {
    fprintf(stderr, "Error opening binary output file: %d\n",f_out_pointer);
    exit(1);
  }
  //first things first, write number of ai channels to binary file
  write(f_out_pointer, &(graph_options->num_channels), sizeof(int));
  }
}

void DAQSystem::PaletteSetup()
{
  QColor slateBlue;
  slateBlue.setRgb(91,115,128); //slateGreen(87,125,128),darkGreen(0,82,97)
  graphColor=slateBlue;
  QBrush graphBrush=QBrush(graphColor);

  QBrush color0Brush=QBrush(color0);
  QBrush grayBrush=QBrush(gray);
  QBrush lightGrayBrush=QBrush(lightGray);
  QBrush darkGrayBrush=QBrush(darkGray);
  QBrush greenBrush=QBrush(green);
  QBrush blueBrush=QBrush(blue);
  QBrush redBrush=QBrush(red);
  QBrush whiteBrush=QBrush(white);
  QBrush slateBlueBrush(slateBlue);
  QBrush darkBlueBrush(darkBlue);

  QColorGroup ScrollbarColors(color0Brush,grayBrush,lightGrayBrush,
			      darkGrayBrush,graphBrush,color0Brush,
			      color0Brush,color0Brush,color0Brush);
  ScrollbarPalette.setDisabled(ScrollbarColors); //for scrollbars
  ScrollbarPalette.setActive(ScrollbarColors);   //for scrollbars
  ScrollbarPalette.setInactive(ScrollbarColors); //for scrollbars

  QColorGroup GreenTextColors(greenBrush,whiteBrush,whiteBrush,lightGrayBrush,
			    whiteBrush,greenBrush,greenBrush,whiteBrush,
			    whiteBrush);
  GreenTextPalette.setDisabled(GreenTextColors);             //for GreenTexts
  GreenTextPalette.setActive(GreenTextColors);               //for GreenTexts
  GreenTextPalette.setInactive(GreenTextColors);             //for GreenTexts

  QColorGroup BlueTextColors(blueBrush,whiteBrush,whiteBrush,lightGrayBrush,
			     whiteBrush,blueBrush,blueBrush,whiteBrush,
			     whiteBrush);
  BlueTextPalette.setDisabled(BlueTextColors);             //for BlueTexts
  BlueTextPalette.setActive(BlueTextColors);               //for BlueTexts
  BlueTextPalette.setInactive(BlueTextColors);             //for BlueTexts

  QColorGroup RedTextColors(redBrush,whiteBrush,whiteBrush,lightGrayBrush,
			    whiteBrush,redBrush,redBrush,whiteBrush,
			    whiteBrush);
  RedTextPalette.setDisabled(RedTextColors);             //for RedTexts
  RedTextPalette.setActive(RedTextColors);               //for RedTexts
  RedTextPalette.setInactive(RedTextColors);             //for RedTexts

  QColorGroup LightGrayTextColors(lightGrayBrush,whiteBrush,whiteBrush,
				  lightGrayBrush,lightGrayBrush,lightGrayBrush,
				  whiteBrush,whiteBrush,whiteBrush);
  LightGrayTextPalette.setDisabled(LightGrayTextColors);//for LightGrayTexts
  LightGrayTextPalette.setActive(LightGrayTextColors);  //for LightGrayTexts
  LightGrayTextPalette.setInactive(LightGrayTextColors);//for LightGrayTexts

  QColorGroup ActiveRecordDataTextColors=BlueTextColors;
  QColorGroup InactiveRecordDataTextColors=LightGrayTextColors;

  RecordDataTextPalette.setDisabled(InactiveRecordDataTextColors);
  RecordDataTextPalette.setActive(ActiveRecordDataTextColors);
  RecordDataTextPalette.setInactive(InactiveRecordDataTextColors);

  RecordDataButtonOnPalette.setDisabled(ActiveRecordDataTextColors);
  RecordDataButtonOnPalette.setActive(ActiveRecordDataTextColors);
  RecordDataButtonOnPalette.setInactive(ActiveRecordDataTextColors);
  RecordDataButtonOffPalette.setDisabled(InactiveRecordDataTextColors);
  RecordDataButtonOffPalette.setActive(InactiveRecordDataTextColors);
  RecordDataButtonOffPalette.setInactive(InactiveRecordDataTextColors);
}

QRect DAQSystem::SpikeBox()
{
  return graph_params[0].spike_frame->frameGeometry();

}

QRect DAQSystem::GraphControls()
{
  return graph_params[0].graph_controls->frameGeometry();
}

QString DAQSystem::TimerValue()
{
  return timer_lcd->text();
}

void DAQSystem::LogSetup() {

  log = new QMultiLineEdit(this, "log");
  int log_x = GraphControls().x();
  int log_y = BtwnButtons; 
  if(graph_options->show_spike)
    {
      log_y += SpikeBox().y()+SpikeBox().height();
    }
  else log_y += GraphControls().y() + GraphControls().height() +BtwnButtons;

  log->setGeometry(log_x, log_y, BoxWidth, 12*ButtonHeight);
  log->insertLine("LOG", 0); 
  log->setCursorPosition(1, 0);
  QString file = graph_options->template_file;
  QFile f(file);
  f.flush();
  if ( f.open(IO_ReadOnly) ) {    // file opened successfully - LG
    QTextStream in( &f );        
    int count = 2;             //increments line number - LG
    while(!in.eof())
      {
	log->insertLine(in.readLine(), count);
	    count ++;
      }
    f.close();
  }
  
  QHBox *log_box = new QHBox(this, "log_box", 0, TRUE); 
  log_box->setGeometry(log_x, log_y+log->height()+BtwnButtons, BoxWidth, ButtonHeight);
  log_box->setSpacing(0);
  log_save = new QPushButton(log_box);
  log_save->setText("Save");
  log_save->setFixedSize(BoxWidth/2, ButtonHeight);
  connect(log_save, SIGNAL(clicked()), this, SLOT(SaveLog())); 
  
  log_timer =  new QPushButton(log_box);
  log_timer->setText("Time");
  log_timer->setFixedSize(BoxWidth/2, ButtonHeight);
  connect(log_timer, SIGNAL(clicked()), this, SLOT(InsertTime()));

}

void DAQSystem::InsertTime()
{
  int line, col;
  log->getCursorPosition(&line, &col);
  log->insertAt(TimerValue(), line, col);
}

void DAQSystem::SaveLog() {
  QString line;
  QString file = graph_options->output_ascii;
  file.truncate(file.length()-3); //removes "dat"
  file+="log";
  QFile f(file);
  if ( f.open(IO_WriteOnly) ) {    // file opened successfully
    QTextStream t( &f );        // use a text stream
    for(int iii=0; iii<log->numLines(); iii++)
      {
	line = log->textLine(iii);
	t << line << endl;
      }
  }
  f.close();
}


void DAQSystem::WhatsThisSetup() {

  QWhatsThis::add(timer_lcd, timer_text);
  QWhatsThis::add(record_data_button, record_text);
  QWhatsThis::add(channel_pulldown, channel_pulldown_text);
  QWhatsThis::add(log, log_text);
  QWhatsThis::add(log_save, log_save_text);
  QWhatsThis::add(log_timer, log_timer_text);

  for(int iii=0; iii<graph_options->num_channels; iii++)
    {
      QWhatsThis::add(graph_params[iii].pause_button, pause_button_text);
      QWhatsThis::add(graph_params[iii].pause_group, pause_group_text);
      QWhatsThis::add(graph_params[iii].pause_all, pause_all_text);
      QWhatsThis::add(graph_params[iii].gain_pulldown, gain_pulldown_text);
      if(graph_options->show_spike)
	{
	  QWhatsThis::add(graph_params[iii].polarity_group, polarity_button_text);
	  QWhatsThis::add(graph_params[iii].polarity_button_label, polarity_button_text);
	  QWhatsThis::add(graph_params[iii].threshold_scroller, threshold_scroller_text);  
	  QWhatsThis::add(graph_params[iii].blanking_scroller, blanking_scroller_text);
	  QWhatsThis::add(graph_params[iii].scroller_label, threshold_scroller_text);  
	  QWhatsThis::add(graph_params[iii].blanking_scroller_label, blanking_scroller_text);
	  QWhatsThis::add(graph_params[iii].spike_indicator, spike_indicator_text);
	  QWhatsThis::add(graph_params[iii].spike_indicator_label, spike_indicator_text);
	}
    }
}

void DAQSystem::WhatsThis()
{
  QWhatsThis::enterWhatsThisMode();
}

void DAQSystem::QuitDAQSystem() {
  f_out.close();

#ifdef DAQ_DEBUG_FLAG
  debug ("quitting now from QuitDAQSystem: closed f_out and /dev/mem \n");
#endif DAQ_DEBUG_FLAG

  exit(0);
}

DAQSystem::~DAQSystem() { //destructor
  delete timer;

  cout << "~DAQSystem" << endl;

}

