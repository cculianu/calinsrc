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

const QString DAQSettings::fileFormatExts [2] = {
  ".nds",
  ".gz"
};


/* Constructs a map with daq_system specific default settings */
DAQSettings::DAQMasterDefaults::DAQMasterDefaults()
{
  DAQMasterDefaults & me = *this;
  /* Daq Master Settings Constructor 
     Setup the master settings map for the daq_system-specific class
     this map also has default settings in case they aren't found in the 
     config file */
  me [SECTION_NAME][ KEY_TEMPLATE_FILE_NAME ]
    = QString(DAQ_LOG_TEMPLATES_PREFIX) + "/default.log";
  me [SECTION_NAME][ KEY_DEVICE ] = "/dev/comedi0";
  me [SECTION_NAME][ KEY_FILE_SOURCE_FILE_NAME ] = "/dev/null";
  me [SECTION_NAME][ KEY_DEFAULT_INPUT_SOURCE ] = QString::number((int)Comedi);
  me [SECTION_NAME][ KEY_SHOW_CONFIG_ON_STARTUP ] = QString::number((int)true);
  me [SECTION_NAME][ KEY_DATA_FILE ] = QString(DAQ_DATA_PREFIX) + "data.nds";
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
  parseAllWindowProfiles();
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

void DAQSettings::clearWindowSettings()
{
  set<uint> windows(windowSettingChannels());
  set<uint>::iterator it;
  static const QRect nullqr;

  /* now clear 'em all */
  for (it = windows.begin(); it != windows.end(); it++) 
    setWindowSetting(*it, nullqr);   
}

const
DAQChannelParams &
DAQSettings::getChannelParameters(uint channel_number) const
{
  map<uint, DAQChannelParams>::const_iterator i = channelParams.find(channel_number);

  if (i != channelParams.end()) {
    return i->second;
  }

  return DAQChannelParams::null;
}

void
DAQSettings::setChannelParameters(uint channel_number, 
                                  const DAQChannelParams & cp)
{
  if (cp.isNull()) {
    channelParams.erase(channel_number);
  } else {
    channelParams [ channel_number ] = cp;
  }
  generateChannelParametersString();
}

void 
DAQSettings::clearChannelParameters()
{
  map<uint, DAQChannelParams>::iterator it;
  set<uint> ids;
  set<uint>::iterator sit;

  for (it = channelParams.begin(); it != channelParams.end(); it++) 
    ids.insert(it->first);
  
  for (sit = ids.begin(); sit != ids.end(); sit++) 
    setChannelParameters(*sit, DAQChannelParams::null); /* set them all to null */
  
}

vector<QString>
DAQSettings::windowSettingProfiles() const
{
  map<QString, WindowSettingProfile>::const_iterator it;
  vector<QString> ret;

  for(it = _windowSettingProfiles.begin(); it != _windowSettingProfiles.end();
      it++) 
    ret.push_back(it->first);
  return ret;
}

DAQSettings::WindowSettingProfile DAQSettings::WindowSettingProfile::null;

DAQSettings::WindowSettingProfile 
DAQSettings::getWindowSettingProfile(QString name) const
{
  map<QString, WindowSettingProfile>::const_iterator it = 
    _windowSettingProfiles.find(name);

  if (it != _windowSettingProfiles.end()) return it->second;
  return WindowSettingProfile::null;
}

void DAQSettings::setWindowSettingProfile(const WindowSettingProfile &p)
{
  QString wss = "", cps = "";
  QString section = QString(WP_SECTION_NAME_PREFIX) + p.name;

  if (p.isNull()) {
    _windowSettingProfiles.erase(p.name);    
  } else {
    _windowSettingProfiles [ p.name ] = p;
    wss = generateWindowSettingsString(p.windowSettings);  
    cps = generateChannelParametersString(p.channelParams);
  }

  if (p.name.isNull()) return;

  settingsMap [section] [ KEY_WP_CHANNEL_PARAMS ] = cps;
  settingsMap [section] [ KEY_WP_CHAN_WIN_SETTINGS ] = wss;

  dirtySettings[section].insert(QString(KEY_WP_CHANNEL_PARAMS));
  dirtySettings[section].insert(QString(KEY_WP_CHAN_WIN_SETTINGS));
}

void DAQSettings::renameWindowSettingProfile(QString newname, QString oldname)
{
  map<QString, WindowSettingProfile>::iterator it 
    = _windowSettingProfiles.find(oldname);

  if (it != _windowSettingProfiles.end()) {
    WindowSettingProfile p = it->second;

    p.setNull(true);
    setWindowSettingProfile(p);
    p.setNull(false);
    p.name = newname;
    setWindowSettingProfile(p);    
  }

}

void DAQSettings::deleteWindowSettingProfile(QString name)
{
  map<QString, WindowSettingProfile>::iterator it 
    = _windowSettingProfiles.find(name);

  if (it != _windowSettingProfiles.end()) {
    WindowSettingProfile p = it->second;

    p.setNull(true);

    setWindowSettingProfile(p);
  }
}

void DAQSettings::currentToProfile(QString name)
{
  WindowSettingProfile p;

  p.setNull(false);
  p.name = name;
  p.channelParams = channelParams;
  p.windowSettings = windowSettings;

  setWindowSettingProfile(p);
}

void
DAQSettings::parseWindowSettings()
{
  windowSettings.clear(); /* empty it out */
  windowSettings = parseWindowSettings(settingsMap [SECTION_NAME] [ KEY_CHAN_WIN_SETTINGS ]);
}

map<uint, QRect>
DAQSettings::parseWindowSettings(QString ws)
{
  map<uint, QRect> ret; 

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
    ret[ chan ] = QRect(x,y,w,h); // add channel to map
  }

  return ret;
}

void
DAQSettings::generateWindowSettingsString() 
{
  QString out(generateWindowSettingsString(windowSettings));

  settingsMap [SECTION_NAME] [ KEY_CHAN_WIN_SETTINGS ] = out;
  dirtySettings[SECTION_NAME].insert(KEY_CHAN_WIN_SETTINGS);
}

QString
DAQSettings::generateWindowSettingsString(const map<uint, QRect> & ws) 
{
  QString out;

  map<uint, QRect>::const_iterator i;
  for (i = ws.begin(); i != ws.end(); i++) {
    uint chan = i->first;
    const QRect & r = i->second;
    out += 
      QString().setNum(chan) + ":"
      + QString().setNum(r.x()) + "," 
      + QString().setNum(r.y()) + ","
      + QString().setNum(r.width()) + ","
      + QString().setNum(r.height()) + ";";
  }
  return out;
}

void
DAQSettings::parseChannelParameters()
{
  channelParams.clear(); /* empty it out */
  channelParams = parseChannelParameters(settingsMap [SECTION_NAME] [ KEY_CHANNEL_PARAMS ]);
}

map<uint, DAQChannelParams>
DAQSettings::parseChannelParameters(QString cp)
{
  map<uint, DAQChannelParams> ret;

  QRegExp chanparmsRE("(\\d+):(\\d+),(\\d+),(\\d+),(-?\\d+[.0-9e-]*),(\\d+),(\\d+)(,(\\d+),(\\d+))?;");
  int curr_match = 0, len = 0;
  while ( (curr_match = chanparmsRE.match(cp, curr_match+len, &len)) > -1 ) {
    DAQChannelParams c; 
    int chan = -1;
    QStringList caps = chanparmsRE.capturedTexts();

    chan = caps[1].toInt();
    c.secondsVisible = caps[2].toInt(); // parse number
    c.rangeSetting = caps[3].toInt();
    c.spikeOn = caps[4].toInt();
    c.spikeThold = caps[5].toDouble();
    c.spikePolarity = (caps[6].toInt() == Negative ? Negative : Positive);
    c.spikeBlanking = caps[7].toInt();
    if (caps[8].length()) {
      /* this is for optional channel params (these params _didn't_ exist in
         legacy daq_system code so we optionally parse them here */
      c.axesOn = caps[9].toInt();
      c.statusBarOn = caps[10].toInt();
    }

    c.setNull(false);
    ret[ chan ] = c; // add channel params to map
  }

  return ret;
}

void
DAQSettings::generateChannelParametersString()
{
  QString out(generateChannelParametersString(channelParams));

  settingsMap [SECTION_NAME] [ KEY_CHANNEL_PARAMS ] = out;
  dirtySettings[SECTION_NAME].insert(KEY_CHANNEL_PARAMS);
}

QString
DAQSettings::
generateChannelParametersString(const map<uint, DAQChannelParams> & cp)
{
  QString out;

  map<uint, DAQChannelParams>::const_iterator i;
  for (i = cp.begin(); i != cp.end(); i++) {
    uint chan = i->first;
    const DAQChannelParams & c = i->second;
    out += 
      QString::number(chan) + ":" +
      QString::number(c.secondsVisible) + "," +
      QString::number(c.rangeSetting) + "," +
      QString::number((short)c.spikeOn) + "," +
      QString::number(c.spikeThold) + "," +
      QString::number((short)c.spikePolarity) + "," +
      QString::number(c.spikeBlanking) + "," +
      QString::number((short)c.axesOn) + "," +
      QString::number((short)c.statusBarOn) + ";";
  }
  return out;
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

void DAQSettings::parseAllWindowProfiles()
{
  SettingsMap::const_iterator it;
  static const QString section_name_prefix(WP_SECTION_NAME_PREFIX);

  _windowSettingProfiles.clear();

  /* Find all the sections beginning with the window profile prefix */
  for (it = settingsMap.begin(); it != settingsMap.end(); it++) {
    QString thissec = it->first.stripWhiteSpace();
    if (thissec.startsWith(section_name_prefix)) {
      WindowSettingProfile newwsp;
      newwsp.setNull(false);
      newwsp.name = thissec.mid(section_name_prefix.length());
      newwsp.windowSettings = parseWindowSettings(settingsMap[it->first][KEY_WP_CHAN_WIN_SETTINGS]);
      newwsp.channelParams = parseChannelParameters(settingsMap[it->first][KEY_WP_CHANNEL_PARAMS]);
      if (!newwsp.name.isNull() && newwsp.windowSettings.size() 
          && newwsp.channelParams.size() )
        _windowSettingProfiles[newwsp.name] = newwsp;
    }
  }
}
