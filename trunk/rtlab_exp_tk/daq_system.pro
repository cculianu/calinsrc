#
# edit the below line to specify the location of the rtl.mk files created
# when you compiled the rt-linux kernel 
#
#TODO: Have ONE unified Makefile.tmake/template combo that generates both
#      Makefile.daq_system and Makefile.rt_process based on the common
#      config options and file paths. 
TEMPLATE    = app
CONFIG      =	qt warn_on debug #release thread
INCLUDEPATH =   
HEADERS     =	config.h common.h shared_stuff.h daq_system.h configuration.h settings.h daq_settings.h probe.h exception.h comedi_device.h sample_source.h sample_reader.cpp producer_consumer.h sample_consumer.h sample_writer.h shm.h ecggraph.h ecggraphcontainer.h simple_text_editor.h profile.h dsdstream.h plugin.h spike_polarity.h layer_renderer.h tweaked_mbuff.h tempfile.h sample_spooler.h output_file_w.h comedi_coprocess.h daq_mime_sources.h html_browser.h daq_images.h daq_help_browser.h searchable_combo_box.h daq_graph_controls.h daq_channel_params.h scanproc.h add_channel.xpm daq_system.xpm plugins.xpm spike_plus.xpm back.xpm log.xpm print.xpm synch.xpm channel.xpm pause.xpm quit.xpm timestamp.xpm configuration.xpm play.xpm spike_minus.xpm wintemplates.xpm
SOURCES     =	main.cpp daq_system.cpp configuration.cpp settings.cpp daq_settings.cpp probe.cpp exception.cpp comedi_device.cpp sample_source.cpp sample_reader.cpp sample_writer.cpp shm.cpp ecggraph.cpp ecggraphcontainer.cpp simple_text_editor.cpp common.cpp profile.cpp dsdstream.cpp dsdstream_inner.cpp layer_renderer.cpp tempfile.cpp sample_spooler.cpp output_file_w.cpp comedi_coprocess.cpp daq_mime_sources.cpp html_browser.cpp searchable_combo_box.cpp daq_images.cpp daq_help_browser.cpp daq_graph_controls.cpp daq_channel_params.cpp scanproc.c
TARGET      =	daq_system
DEFINES     =   #DAQ_SYSTEM_PROFILE_SLEEPTIME_CODE #QT_THREAD_SUPPORT
LIBS        =   -lcomedi -ldl -export-dynamic -lpthread -lz
TMAKE_LIBS_QT = -lqt #-lqt-mt
