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
#include <iterator>
#include <set>
#include <map>
#include <qstring.h>
#include <qregexp.h>
#include <stdlib.h>
#include "daq_settings.h"

const QString DAQSettings::defaultConfigFileName 
  = DAQ_SYSTEM_USER_DIR + "/config.ini";

const DAQSettings::DAQMasterDefaults DAQSettings::daqMasterSettings;

/* Constructs a map with daq_system specific default settings */
DAQSettings::DAQMasterDefaults::DAQMasterDefaults()
{
  DAQMasterDefaults & me = *this;
  /* Daq Master Settings Constructor 
     Setup the master settings map for the daq_system-specific class
     this map also has default settings in case they aren't found in the 
     config file */
  me [SECTION_NAME][ KEY_TEMPLATE_FILE_NAME ]
    = QString(DAQ_RESOURCES_PREFIX) + "/default.log";
  me [SECTION_NAME][ KEY_DEVICE ] = "/dev/comedi0";
  me [SECTION_NAME][ KEY_FILE_SOURCE_FILE_NAME ] = "/dev/null";
  me [SECTION_NAME][ KEY_DEFAULT_INPUT_SOURCE ] = QString::number((int)Comedi);
  me [SECTION_NAME][ KEY_SHOW_CONFIG_ON_STARTUP ] = QString::number((int)true);
  me [SECTION_NAME][ KEY_DATA_FILE ] = QString(DAQ_DATA_PREFIX) + "data.bin";
  me [SECTION_NAME][ KEY_DATA_FORMAT ] = QString::number((int)Binary);

  /* the following is a  'special' setting.  It is basically
     a special-format list of the format: 
     [channel-id:x,y,width,height[{;}...]] */
  me [SECTION_NAME][ KEY_CHAN_WIN_SETTINGS ] = "0:0,0,300,400;";

  /* channel params settings */
  me [SECTION_NAME][ KEY_CHANNEL_PARAMS ] = "";

  /* paper size settings */
  me [SECTION_NAME][ KEY_PAGE_SIZE ] = QString::number(static_cast<int>(QPrinter::Letter));
}

DAQSettings::DAQSettings(const char *filename = 0)
{
  masterSettings = &daqMasterSettings;

  if (!filename)
    filename = defaultConfigFileName.latin1();

  setConfigFileName(filename);
  parseSettings();
  parseWindowSettings();
  parseChannelParameters();
}

const QString &
DAQSettings::getDevice() const
{
  return settingsMap.find(SECTION_NAME)->second.find(KEY_DEVICE)->second;
}


void
DAQSettings::setDevice(const QString & dev)
{
  settingsMap[SECTION_NAME][KEY_DEVICE] = dev;
  dirtySettings[SECTION_NAME].insert(KEY_DEVICE);
}

const QString &
DAQSettings::getTemplateFileName() const
{
  return settingsMap.find(SECTION_NAME)->second.find(KEY_TEMPLATE_FILE_NAME)->second;
}

void
DAQSettings::setTemplateFileName(const QString & fileName) 
{
  settingsMap[SECTION_NAME][KEY_TEMPLATE_FILE_NAME] = fileName;
  dirtySettings[SECTION_NAME].insert(KEY_TEMPLATE_FILE_NAME);
}

const QString & 
DAQSettings::getFileSourceFileName() const
{
  return settingsMap.find(SECTION_NAME)->second.find(KEY_FILE_SOURCE_FILE_NAME)->second;
}


void 
DAQSettings::setFileSourceFileName(const QString & fileName)
{
  settingsMap[SECTION_NAME][KEY_FILE_SOURCE_FILE_NAME] = fileName;
  dirtySettings[SECTION_NAME].insert(KEY_FILE_SOURCE_FILE_NAME);
}


DAQSettings::InputSource 
DAQSettings::getInputSource() const
{
  InputSource ret = Comedi;
  bool ok;
  int val = settingsMap.find(SECTION_NAME)->second.find(KEY_DEFAULT_INPUT_SOURCE)->second.toInt(&ok);

  if (ok && val > invalid_source_low && val < invalid_source_high)
    ret = (InputSource)val;

  return ret;
}

void 
DAQSettings::setInputSource(InputSource source)
{  
  settingsMap[SECTION_NAME][KEY_DEFAULT_INPUT_SOURCE] = QString().setNum((int)source);
  dirtySettings[SECTION_NAME].insert(KEY_DEFAULT_INPUT_SOURCE);
}

bool
DAQSettings::getShowConfigOnStartup() const
{
  return (settingsMap.find(SECTION_NAME)->second.find(KEY_SHOW_CONFIG_ON_STARTUP)->second.toShort() != 0);
}

void 
DAQSettings::setShowConfigOnStartup(bool yesorno)
{
  settingsMap[SECTION_NAME][KEY_SHOW_CONFIG_ON_STARTUP] = QString().setNum((short)yesorno);
  dirtySettings[SECTION_NAME].insert(KEY_SHOW_CONFIG_ON_STARTUP);
}

const QString &
DAQSettings::getDataFile() const
{
  return settingsMap.find(SECTION_NAME)->second.find(KEY_DATA_FILE)->second;
}

void 
DAQSettings::setDataFile(const QString & fileName)
{
  settingsMap[SECTION_NAME][KEY_DATA_FILE] = fileName;
  dirtySettings[SECTION_NAME].insert(KEY_DATA_FILE);
}

DAQSettings::DataFileFormat
DAQSettings::getDataFileFormat() const
{
  DataFileFormat ret = Binary;
  bool ok;
  int val = settingsMap.find(SECTION_NAME)->second.find(KEY_DATA_FORMAT)->second.toInt(&ok);

  if (ok && val > invalid_format_low && val < invalid_format_high)
    ret = (DataFileFormat)val;

  return ret;
}

void 
DAQSettings::setDataFileFormat(DataFileFormat format)
{  
  settingsMap[SECTION_NAME][KEY_DATA_FORMAT] = QString::number((int)format);
  dirtySettings[SECTION_NAME].insert(KEY_DATA_FORMAT);
}

const
QRect &
DAQSettings::getWindowSetting(uint channel_number) const
{
  static QRect nullRect;
  
  if (windowSettings.find(channel_number) != windowSettings.end()) {
    return windowSettings.find(channel_number)->second;
  }

  return nullRect;
}

void
DAQSettings::setWindowSetting(uint channel_number, const QRect & rect)
{
  if (rect.isNull()) {
    windowSettings.erase(channel_number);
  } else {
    windowSettings [ channel_number ] = rect;
  }
  generateWindowSettingsString();
}

set<uint>
DAQSettings::windowSettingChannels() const
{
  set<uint> s;

  for (map<uint, QRect>::const_iterator i = windowSettings.begin(); 
       i != windowSettings.end(); i++) {
    s.insert(i->first);
  }
  return s;
}

const
DAQSettings::ChannelParams &
DAQSettings::getChannelParameters(uint channel_number) const
{
  static const ChannelParams nullCP;

  map<uint, ChannelParams>::const_iterator i = channelParams.find(channel_number);

  if (i != channelParams.end()) {
    return i->second;
  }

  return nullCP;
}

void
DAQSettings::setChannelParameters(uint channel_number, 
                                  const ChannelParams & cp)
{
  if (cp.isNull()) {
    channelParams.erase(channel_number);
  } else {
    channelParams [ channel_number ] = cp;
  }
  generateChannelParametersString();
}

void
DAQSettings::parseWindowSettings()
{
  windowSettings.clear(); /* empty it out */

  const QString & ws(settingsMap [SECTION_NAME] [ KEY_CHAN_WIN_SETTINGS ]);
  QRegExp winsetRE("\\d+:\\d+,\\d+,\\d+,\\d+;");
  int curr_match = 0, len = 0;
  while ( (curr_match = winsetRE.match(ws, curr_match+len, &len)) > -1 ) {
    int chan, x, y, w, h, tmp; chan = x = y = w = h = -1;
    QString match = ws.mid(curr_match, len);
    chan = match.left((tmp = match.find(':'))).toInt(); // parse number
    match = match.mid(tmp + 1); // consume number
    x = match.left((tmp = match.find(','))).toInt(); // parse number
    match = match.mid(tmp + 1); // consume number
    y = match.left((tmp = match.find(','))).toInt(); // parse number
    match = match.mid(tmp + 1); // consume number
    w = match.left((tmp = match.find(','))).toInt(); // parse number
    match = match.mid(tmp + 1); // consume number
    h = match.left((tmp = match.find(';'))).toInt(); // parse number
    match = match.mid(tmp + 1); // consume number
    windowSettings[ chan ] = QRect(x,y,w,h); // add channel to map
  }
}

void
DAQSettings::generateWindowSettingsString() 
{
  QString out;

  map<uint, QRect>::iterator i;
  for (i = windowSettings.begin(); i != windowSettings.end(); i++) {
    uint chan = i->first;
    QRect & r = i->second;
    out += 
      QString().setNum(chan) + ":"
      + QString().setNum(r.x()) + "," 
      + QString().setNum(r.y()) + ","
      + QString().setNum(r.width()) + ","
      + QString().setNum(r.height()) + ";";
  }
  settingsMap [SECTION_NAME] [ KEY_CHAN_WIN_SETTINGS ] = out;
  dirtySettings[SECTION_NAME].insert(KEY_CHAN_WIN_SETTINGS);
  //cerr << "Settings string: " << out << endl;
}

void
DAQSettings::parseChannelParameters()
{
  channelParams.clear(); /* empty it out */

  const QString & cp(settingsMap [SECTION_NAME] [ KEY_CHANNEL_PARAMS ]);
  QRegExp winsetRE("\\d+:\\d+,\\d+,\\d+,-?\\d+.?\\d*,\\d+,\\d+;");
  int curr_match = 0, len = 0;
  while ( (curr_match = winsetRE.match(cp, curr_match+len, &len)) > -1 ) {
    ChannelParams c; 
    int chan, tmp; chan = tmp = -1;
    QString match = cp.mid(curr_match, len);
    chan = match.left((tmp = match.find(':'))).toInt(); // parse number
    match = match.mid(tmp + 1); // consume number
    c.n_secs = match.left((tmp = match.find(','))).toInt(); // parse number
    match = match.mid(tmp + 1); // consume number
    c.range = match.left((tmp = match.find(','))).toInt(); // parse number
    match = match.mid(tmp + 1); // consume number
    c.spike_on = match.left((tmp = match.find(','))).toInt(); // parse number
    match = match.mid(tmp + 1); // consume number
    c.spike_thold = match.left((tmp = match.find(','))).toDouble(); // parse number
    match = match.mid(tmp + 1); // consume number
    c.spike_polarity = match.left((tmp = match.find(','))).toInt(); // parse number
    match = match.mid(tmp + 1); // consume number
    c.spike_blanking = match.left((tmp = match.find(';'))).toInt(); // parse number
    match = match.mid(tmp + 1); // consume number
    c.setNull(false);
    channelParams[ chan ] = c; // add channel params to map
  }
}

void
DAQSettings::generateChannelParametersString()
{
  QString out;

  map<uint, ChannelParams>::iterator i;
  for (i = channelParams.begin(); i != channelParams.end(); i++) {
    uint chan = i->first;
    ChannelParams & c = i->second;
    out += 
      QString::number(chan) + ":" +
      QString::number(c.n_secs) + "," +
      QString::number(c.range) + "," +
      QString::number((short)c.spike_on) + "," +
      QString::number(c.spike_thold) + "," +
      QString::number((short)c.spike_polarity) + "," +
      QString::number(c.spike_blanking) + ";";
  }

  settingsMap [SECTION_NAME] [ KEY_CHANNEL_PARAMS ] = out;
  dirtySettings[SECTION_NAME].insert(KEY_CHANNEL_PARAMS);
}

QPrinter::PageSize 
DAQSettings::getPageSize() const
{
  return static_cast<QPrinter::PageSize>(settingsMap.find(SECTION_NAME)->second.find(KEY_PAGE_SIZE)->second.toInt());
}

void DAQSettings::setPageSize(QPrinter::PageSize pagesize)
{
  settingsMap[SECTION_NAME][KEY_PAGE_SIZE] = QString::number(static_cast<int>(pagesize));
  dirtySettings[SECTION_NAME].insert(KEY_PAGE_SIZE);  
}
