/*
 * This file is part of the RT-Linux Multichannel Data Acquisition System
 *
 * Copyright (C) 2002 Calin Culianu
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
#ifndef _DAQ_SETTINGS_H
#define _DAQ_SETTINGS_H
#include <qrect.h>
#include <qprinter.h>
#include "common.h"
#include "config.h"
#include "settings.h"
#include "comedi_device.h"

#  define DAQ_SYSTEM_USER_DIR QString(getenv("HOME")) + "/." + DAQ_DIRNAME

class DAQSettings: public Settings
{
 public:

  DAQSettings(const char *filename = 0);
  
  enum InputSource {
    invalid_source_low = -1,
    Comedi,
    RTProcess,
    File,
    invalid_source_high    
  };

  enum DataFileFormat {
    invalid_format_low = -1,
    Binary,
    Ascii,
    invalid_format_high    
  };

  /* indexed by the above enum */
  static const QString fileFormatExts [2];

  /* Class used to encapsulate default settings for the daq system.
     Instances of this class should be assigned to Settings::masterSettings,
     as they contain the master list of settings to read from the config file*/
  class DAQMasterDefaults: public Settings::SettingsMap
  {
  public:
    DAQMasterDefaults();  /* constructs a map with daq_system specific
			     default settings */
  };

  /* Configurable Settings Methods
     -----------------------------
     When adding configurable settings, it is important to update this class 
     with methods below, as well as associated members  */
  const QString & getDevice() const;
  void setDevice(const QString & dev);
  
  const QString & getTemplateFileName() const;
  void setTemplateFileName(const QString &fileName);

  const QString & getFileSourceFileName() const;
  void setFileSourceFileName(const QString & fileName);

  InputSource getInputSource() const;
  void setInputSource(InputSource source);

  bool getShowConfigOnStartup() const;
  void setShowConfigOnStartup(bool yesorno);

  const QString & getDataFile() const;
  void setDataFile(const QString & fileName);

  DataFileFormat getDataFileFormat() const;
  void setDataFileFormat(DataFileFormat format);

  /* returns a null QRect (isNull() == true) if specified channel
     has no settings */
  const QRect & getWindowSetting(uint channel_number) const;
  /* to disable a window, pass a null QRect */
  void setWindowSetting(uint channel_number, const QRect & rect);

  /* returns a set of channel-id's that are all channels
     that have window settings.  This is intended to be used in daq system
     as a master list of channels to re-open for the user as a convenience
     on startup. All the windowSettings in this set will return a valid
     QRect in getWindowSettings() */
  set<uint> windowSettingChannels() const;

  /* Struct used by daq system to communicate with the settings class */
  struct ChannelParams {
    uint n_secs;
    uint range;
    bool spike_on;
    double spike_thold;
    bool spike_polarity;
    uint spike_blanking;

    void setNull(bool n) { isnull = n; };
    bool isNull() const { return isnull; };
    ChannelParams() { setNull(true); };
   private:
    bool isnull; // means this channel params struct is ignoreable
  };

  void setChannelParameters(uint channel_number, const ChannelParams & cp);
  const ChannelParams & getChannelParameters(uint channel_number) const;

  void setPageSize(QPrinter::PageSize);
  QPrinter::PageSize getPageSize() const;

 private:

  map<uint, QRect> windowSettings;

  map<uint, ChannelParams> channelParams;

  /* parses the windosettings string and sets up the windowSettings map */
  void parseWindowSettings();
  
  /* generates the windowsettings string to be saved to config file */
  void generateWindowSettingsString();

  /* parses the channel parameters string and sets up the channelParams map */
  void parseChannelParameters();

  /* generates the channel params string to be saved to config file */
  void generateChannelParametersString();

  /* master list of all settings we like to worry about in daq system */
  static const DAQMasterDefaults daqMasterSettings;

  static const QString defaultConfigFileName;

  static const char
    * const SECTION_NAME = "DAQ System Settings",
    * const KEY_TEMPLATE_FILE_NAME = "templateFileName",
    * const KEY_DEVICE = "device",
    * const KEY_FILE_SOURCE_FILE_NAME = "fileSourceFileName",
    * const KEY_DEFAULT_INPUT_SOURCE = "defaultInputSource",
    * const KEY_SHOW_CONFIG_ON_STARTUP = "showConfigOnStartup",
    * const KEY_DATA_FILE = "dataFile",
    * const KEY_DATA_FORMAT = "dataFileDefaultFormat",
    * const KEY_CHAN_WIN_SETTINGS = "channelWindowSettings",
    * const KEY_CHANNEL_PARAMS = "channelParameters",
    * const KEY_PAGE_SIZE = "printerPageSize";

};

#endif
