/*
 * This file is part of the RT-Linux Multichannel Data Acquisition System
 *
 * Copyright (C) 2001 Calin Culianu
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
#ifndef _SETTINGS_H
#define _SETTINGS_H

#include <iterator>
#include <set>
#include <map>
#include <qfile.h>
#include <qstring.h>
#include <qrect.h>
#include <qregexp.h>
#include "comedi_device.h"

/* 
   todo: figure out a nice way to use hashsets on  all this so
   that this class can be configured more easily 

   also: maybe make this class automatically read from
         ConfigurationWindow so that it can automatically update itself??
*/

#  define DAQ_SYSTEM_USER_DIR QString(getenv("HOME")) + "/." + DAQ_DIRNAME

class Settings
{
 public:

  Settings();
  virtual ~Settings(){};

  virtual void parseConfig();
  virtual void saveSettings();
  virtual QString getConfigFileName() const;
  virtual void setConfigFileName(const QString & name); /*  after setting this,
							    typically you need 
							    to call 
							    parseConfig() */
  virtual QString get(const QString & key) const 
    { return settingsMap.find(key)->second; };

 protected:

  set<QString> dirtySettings; 

  map<QString, QString> settingsMap;

  /* a derived class NEEDS to set masterSettings hopefully in constructor! */
  const map<QString, QString> *masterSettings;

  /* Derived classes may need to override these in their constructors! */
  const QRegExp valueRE, lineRE;

/* searches config file for key and  returns it's value or a null QString */
  virtual QString findValue(const QString & key); 

  /* parses a VALID settings line and populates key and value */
  virtual void parseMatchedLine(const QString & matchedline, 
				QString & key, QString & value) const; 

  /* returns a matched substring from line if line matches the lineRE, 
     or otherwise a null QString */
  virtual QString testLine(const QString & line) const;

  QFile configFile;

};


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


  /* Class used to encapsulate default settings for the daq system. 
     Instances of this class should be assigned to Settings::masterSettings,
     as they contain the master list of settings to read from the config file*/
  class DaqMasterDefaults: public map<QString, QString>
  {
  public:
    DaqMasterDefaults();  /* constructs a map with daq_system specific 
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

 private:

  map<uint, QRect> windowSettings;

  /* parses the windosettings string and sets up the windowSettings map */
  void parseWindowSettings();
  
  /* generates the windowsettings string to be saved to config file */
  void generateWindowSettingsString();

  /* master list of all settings we like to worry about in daq system */
  static const DaqMasterDefaults daqMasterSettings; 

  static const QString defaultConfigFileName;

  static const char 
    * const KEY_TEMPLATE_FILE_NAME = "templateFileName",
    * const KEY_DEVICE = "device",
    * const KEY_FILE_SOURCE_FILE_NAME = "fileSourceFileName",
    * const KEY_DEFAULT_INPUT_SOURCE = "defaultInputSource",
    * const KEY_SHOW_CONFIG_ON_STARTUP = "showConfigOnStartup",
    * const KEY_DATA_FILE = "dataFile",
    * const KEY_DATA_FORMAT = "dataFileDefaultFormat",
    * const KEY_CHAN_WIN_SETTINGS = "channelWindowSettings";


};




#endif
