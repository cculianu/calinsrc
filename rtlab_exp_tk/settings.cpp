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

#include <iterator>
#include <set>
#include <map>
#include <qtextstream.h> 
#include <qfile.h>
#include <qstring.h>
#include <qregexp.h>
#include <stdlib.h>
#include "common.h"
#include "config.h"
#include "settings.h"
#include "comedi_device.h"

#ifdef TEST_SETTINGS
int
main (void)
{
  Settings settings;  
  cout << "device is " << settings.getDevice().filename << endl;
  cout << "template file is " << settings.getTemplateFileName() << endl;
  return 0;
}
#endif



const QString DAQSettings::defaultConfigFileName 
  = DAQ_SYSTEM_USER_DIR + "/conf";

const DAQSettings::DaqMasterDefaults DAQSettings::daqMasterSettings; 

/* Constructs a map with daq_system specific default settings */
DAQSettings::DaqMasterDefaults::DaqMasterDefaults()
{
  DaqMasterDefaults & me = *this;
  /* Daq Master Settings Constructor 
     Setup the master settings map for the daq_system-specific class
     this map also has default settings in case they aren't found in the 
     config file */
  me [ KEY_TEMPLATE_FILE_NAME ] 
    = QString(DAQ_RESOURCES_PREFIX) + DAQ_DIRNAME + "/default.log";
  me [ KEY_DEVICE ] = "/dev/comedi0";
  me [ KEY_FILE_SOURCE_FILE_NAME ] = "/dev/null";
  me [ KEY_DEFAULT_INPUT_SOURCE ] = QString().setNum((int)Comedi);
  me [ KEY_SHOW_CONFIG_ON_STARTUP ] = QString().setNum((int)true);
  me [ KEY_DATA_FILE ] = QString(DAQ_DATA_PREFIX) + "data.bin";
  me [ KEY_DATA_FORMAT ] = QString().setNum((int)Binary);

  /* the following is a  'special' setting.  It is basically
     a special-format list of the format: 
     [channel-id:x,y,width,height[{;}...]] */
  me [ KEY_CHAN_WIN_SETTINGS ] = "0:0,0,300,400;";
}



Settings::Settings() 
  :  valueRE ("\"[^\\s=\"]*\""),
     lineRE ("^\\s*[a-zA-Z0-9./_]+\\s*=\\s*" + valueRE.pattern())
{};

void
Settings::setConfigFileName(const QString &fileName)
{
  configFile.setName(fileName);
}

QString 
Settings::getConfigFileName() const
{
  return configFile.name();
}

void
Settings::parseConfig()
{
  map<QString, QString>::const_iterator it;

  // pick up the values
  for (it = masterSettings->begin(); it != masterSettings->end(); it++) {
    QString tmp(findValue(it->first));
    settingsMap[it->first] = ( tmp.isNull() ? it->second : tmp );
  }
  dirtySettings.clear();

}

/* 
   Opens & searches the config file, finds a value for a given key, 
   closes the config file and returns the value it found or QString::null
*/
QString
Settings::findValue(const QString & key) 
{

  QString line, ret;
  
  configFile.open(IO_ReadWrite); /* ReadWrite so that if it doesn't exist, 
				    we can at leat create it as a 0-byter */
  QTextStream fileContents(&configFile);

  while (! (line = fileContents.readLine()).isNull() ) {
    QString matchedLine(testLine(line)), thiskey, thisvalue;
    if ( ! matchedLine.isNull() ) {      
      parseMatchedLine(matchedLine, thiskey, thisvalue); 
      if (thiskey == key) {
	ret = thisvalue;
	goto endmethod;
      }
      
    }
  }
 endmethod:
  configFile.close();
  return ret; // if nothing found, returns a null QString
}

void 
Settings::saveSettings()
{
  QString line, matchedLine,  outBuf, key, value;

  configFile.open(IO_ReadOnly);
  QString theWholeThing(QTextStream(&configFile).read());
  configFile.close(); 

  QTextStream inFile(&theWholeThing, IO_ReadOnly);

  while (! (line = inFile.readLine()).isNull() ) {
    
    matchedLine = testLine(line);

    // replace parameter in config file if they changed
    if ( !matchedLine.isNull() ) {
      parseMatchedLine(matchedLine, key, value);
      if (dirtySettings.count(key)) {
	line = key + " = \"" + settingsMap[key] + "\"";
	dirtySettings.erase(key);
      }      
    }
    outBuf += line + "\n";
  }

  // now append what's left to the end of the file
  set<QString>::iterator it;
  for (it = dirtySettings.begin(); it != dirtySettings.end(); it++) {
    outBuf += *it + " = \"" + settingsMap[*it] + "\" \n";    
  }

  // now, commit changes, overwriting file
  configFile.open(IO_WriteOnly);  
  configFile.writeBlock(outBuf.latin1(), outBuf.length()); 
  configFile.close();
}

/* returns a matched substring from line if line matches the lineRE, 
   or otherwise a null QString */
QString
Settings::testLine(const QString & line) const
{
  int pos, len;

  if ( (pos = lineRE.match(line, 0, &len)) > -1) {
    return line.mid(pos,len);
  } 
  return QString();
}

void
Settings::parseMatchedLine(const QString & matchedLine, 
			   QString & key, QString & value) const
{
  int eq_idx = matchedLine.find('='), pos, len;
  key = matchedLine.left(eq_idx);
  key.replace(QRegExp(" "),""); // strip/trim spaces
  pos = valueRE.match(matchedLine, eq_idx, &len);
  value = matchedLine.mid(pos,len);
  value.replace(QRegExp("\""),""); // ok, get rid of the "
}


DAQSettings::DAQSettings(const char *filename = 0)
{
  masterSettings = &daqMasterSettings;

  if (!filename)
    filename = defaultConfigFileName.latin1();

  setConfigFileName(filename);
  parseConfig();
  parseWindowSettings();
}

const QString &
DAQSettings::getDevice() const
{
  return settingsMap.find(KEY_DEVICE)->second;
}


void
DAQSettings::setDevice(const QString & dev)
{
  settingsMap[KEY_DEVICE] = dev;
  dirtySettings.insert(KEY_DEVICE);
}

const QString &
DAQSettings::getTemplateFileName() const
{
  return settingsMap.find(KEY_TEMPLATE_FILE_NAME)->second;  
}

void
DAQSettings::setTemplateFileName(const QString & fileName) 
{
  settingsMap[KEY_TEMPLATE_FILE_NAME] = fileName;
  dirtySettings.insert(KEY_TEMPLATE_FILE_NAME);
}

const QString & 
DAQSettings::getFileSourceFileName() const
{
  return settingsMap.find(KEY_FILE_SOURCE_FILE_NAME)->second;
}


void 
DAQSettings::setFileSourceFileName(const QString & fileName)
{
  settingsMap[KEY_FILE_SOURCE_FILE_NAME] = fileName;
  dirtySettings.insert(KEY_FILE_SOURCE_FILE_NAME);
}


DAQSettings::InputSource 
DAQSettings::getInputSource() const
{
  InputSource ret = Comedi;
  bool ok;
  int val = settingsMap.find(KEY_DEFAULT_INPUT_SOURCE)->second.toInt(&ok);

  if (ok && val > invalid_source_low && val < invalid_source_high)
    ret = (InputSource)val;

  return ret;
}

void 
DAQSettings::setInputSource(InputSource source)
{  
  settingsMap[KEY_DEFAULT_INPUT_SOURCE] = QString().setNum((int)source);
  dirtySettings.insert(KEY_DEFAULT_INPUT_SOURCE);
}

bool
DAQSettings::getShowConfigOnStartup() const
{
  return (settingsMap.find(KEY_SHOW_CONFIG_ON_STARTUP)->second.toShort() != 0);
}

void 
DAQSettings::setShowConfigOnStartup(bool yesorno)
{
  settingsMap[KEY_SHOW_CONFIG_ON_STARTUP] = QString().setNum((short)yesorno);
  dirtySettings.insert(KEY_SHOW_CONFIG_ON_STARTUP);
}

const QString &
DAQSettings::getDataFile() const
{
  return settingsMap.find(KEY_DATA_FILE)->second;
}

void 
DAQSettings::setDataFile(const QString & fileName)
{
  settingsMap[KEY_DATA_FILE] = fileName;
  dirtySettings.insert(KEY_DATA_FILE);
}

DAQSettings::DataFileFormat
DAQSettings::getDataFileFormat() const
{
  DataFileFormat ret = Binary;
  bool ok;
  int val = settingsMap.find(KEY_DATA_FORMAT)->second.toInt(&ok);

  if (ok && val > invalid_format_low && val < invalid_format_high)
    ret = (DataFileFormat)val;

  return ret;
}

void 
DAQSettings::setDataFileFormat(DataFileFormat format)
{  
  settingsMap[KEY_DATA_FORMAT] = QString().setNum((int)format);
  dirtySettings.insert(KEY_DATA_FORMAT);
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

void
DAQSettings::parseWindowSettings()
{
  windowSettings.clear(); /* empty it out */

  const QString & ws(settingsMap [ KEY_CHAN_WIN_SETTINGS ]);
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
  settingsMap [ KEY_CHAN_WIN_SETTINGS ] = out;
  dirtySettings.insert(KEY_CHAN_WIN_SETTINGS);
  //cerr << "Settings string: " << out << endl;
}
