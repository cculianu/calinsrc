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
#include "common.h"
#include "config.h"
#include "settings.h"


static const char   *lineRE    = "^\\s*([a-zA-Z0-9./_]+)\\s*=\\s*\"?([^\\n\\r\\f=\"]*)\"?",
                    *sectionRE = "\\s*\\[\\s*([^\\]]+)\\]";

Settings::Settings() : lineRE(::lineRE), sectionRE(::sectionRE)
{ init(); };

Settings::Settings(QIODevice *d) : lineRE(::lineRE), sectionRE(::sectionRE)
{
  init();
  setConfigDevice(d);
};

void Settings::init()
{
  _currentSection = QString::null;
  masterSettings = 0;
  configFile = 0;
  need_to_delete_configFile = false;

}
Settings::~Settings()
{
  maybeDeleteConfigFile();
};

void
Settings::maybeDeleteConfigFile()
{
  if (need_to_delete_configFile) {
    delete configFile;
    configFile = 0;
    need_to_delete_configFile = false;
  }
}

QIODevice::Offset
Settings::length() const
{
  QIODevice::Offset ret = 0;
  if (getConfigDevice()) ret = getConfigDevice()->size();
  return ret;
}

void
Settings::setConfigFileName(const QString &fileName)
{
  maybeDeleteConfigFile();
  configFile = new QFile(fileName);
  need_to_delete_configFile = true;
}

QString 
Settings::getConfigFileName() const
{
  QFile *f;
  if ( (f = dynamic_cast<QFile *>(configFile)) ) {
    return f->name();
  }
  return QString::null;
}

  // alternate usage, pass in a QIODevice
void
Settings::setConfigDevice(QIODevice * device)
{
  maybeDeleteConfigFile();
  configFile = device;
}

QIODevice *
Settings::getConfigDevice() const
{
  return configFile;
}

QString
Settings::get(const QString & key) const
{
  return get(_currentSection, key);
}

void
Settings::put(const QString & key, const QString & value)
{
  put(_currentSection, key, value);
}

QString
Settings::get(const QString & section, const QString & key) const
{
  SettingsMap::const_iterator sec_it = settingsMap.find(section);

  if (sec_it != settingsMap.end() && (sec_it->second.count(key) > 0) )
    return sec_it->second.find(key)->second;

  return QString();
}

void
Settings::put(const QString & section, const QString & key, const QString & value)
{
  settingsMap[section][key] = value;
  dirtySettings[section].insert(key);
}

Settings::Section Settings::getSection(const QString & section_name) const
{
  if (settingsMap.count(section_name) > 0)
    return settingsMap.find(section_name)->second;
  return Section();
}

vector<QString> Settings::sections() const
{
  SettingsMap::const_iterator it;
  vector<QString> ret;

  for (it = settingsMap.begin(); it != settingsMap.end(); it++) 
    ret.push_back(it->first);
  
  return ret;
}

Settings & Settings::operator<<(const Settings & from)
{
  return merge(from);
}

Settings & Settings::merge(const Settings & from)
{
  vector<QString> from_secs = from.sections();
  vector<QString>::iterator it;

  for ( it = from_secs.begin(); it != from_secs.end(); it++)
    mergeSection(*it, from.getSection(*it));

  return *this;
}

void Settings::putSection(const QString & section_name, const Section & section)
{
  settingsMap[section_name] = section;
  for (Section::const_iterator it = section.begin(); it != section.end(); it++)
    dirtySettings[section_name].insert(it->first);
}

void Settings::mergeSection(const QString & section_name, 
                            const Section & section)
{
  Section::const_iterator it;

  setSection(section_name);
  
  for (it = section.begin(); it != section.end(); it++)
    put(it->first, it->second);
}

void
Settings::parseSettings()
{
  dirtySettings.clear();
  settingsMap.clear();

  if (masterSettings) {
    SettingsMap allSettings = readAll();
    SettingsMap::const_iterator master;
    Section::const_iterator subMaster;
    QString section, key, value;

    // pick up the values
    for (master = masterSettings->begin(); master != masterSettings->end(); master++) {
      section = master->first;
      for (subMaster = master->second.begin(); subMaster != master->second.end(); subMaster++) {
          key = subMaster->first;
          value = subMaster->second;
          settingsMap[section][key] = allSettings[section][key];
          if ( settingsMap[section][key].isNull() ) {
            settingsMap[section][key] = value;
            dirtySettings[section].insert(key);
          }
      }
    }

  } else {
    settingsMap = readAll();
  }


}


Settings::SettingsMap
Settings::readAll()
{
    SettingsMap ret;
    QString line, matchedLine, thisKey, thisValue;

    configFile->open(IO_ReadWrite);  /* ReadWrite so that if it doesn't exist,
                                        we can at leat create it as a 0-byter */
    QTextStream fileContents(configFile);
    while (! (line = fileContents.readLine()).isNull()) {
      matchedLine = testSectionLine(line);
      if ( !matchedLine.isNull()) {
        _currentSection = matchedLine;
        continue;
      }
      matchedLine = testLine(line);
      if ( !matchedLine.isNull() ) {
        parseMatchedLine(matchedLine, thisKey, thisValue);
        ret[currentSection()][thisKey] = thisValue;
      }
    }

    configFile->close();
    return ret;
}

void
Settings::saveSettings()
{
  QString line, matchedLine, key, value, outBuf("");
  DirtyKeys::iterator it;

  configFile->open(IO_ReadOnly);
  QString theWholeThing(QTextStream(configFile).read());
  configFile->close();

  _currentSection = QString::null;
  QTextStream inFile(&theWholeThing, IO_ReadOnly);
  while (! (line = inFile.readLine()).isNull() ) {
    if ( !(matchedLine = testSectionLine(line)).isNull() ) {
      // we have a new section so append what's still 'dirty' for this section to the end of the section that is now ending
      for (it = dirtySettings[currentSection()].begin(); it != dirtySettings[currentSection()].end(); it++) {
        outBuf += *it + " = " + settingsMap[currentSection()][*it] + " \n";
      }
      dirtySettings[currentSection()].clear();
      _currentSection = matchedLine;

    } else if ( ! (matchedLine = testLine(line)).isNull()) {
      parseMatchedLine(matchedLine, key, value);
      if (dirtySettings[currentSection()].count(key)) {
        line = key + " = " + settingsMap[currentSection()][key];
        dirtySettings[currentSection()].erase(key);
      }      
    }
    outBuf += line + "\n";
  }

  // if there are still any dirty settings left, put what is left over at the end
  DirtyMap::iterator ds_it;
  for (ds_it = dirtySettings.begin(); ds_it != dirtySettings.end(); ds_it++) {
    if (ds_it->second.size() == 0) continue;
    QString section = ds_it->first;
    if (section != currentSection()) outBuf += QString("[") + section.latin1() + "]\n";
    for (it = ds_it->second.begin(); it != ds_it->second.end(); it++)
      outBuf += *it + " = " + settingsMap[section][*it] + "\n";
    _currentSection = section;
  }


  // now, commit changes, overwriting file
  configFile->open(IO_WriteOnly | IO_Truncate);
  configFile->writeBlock(outBuf, outBuf.length());
  configFile->close();
  dirtySettings.clear();
}

/* returns a matched substring from line if line matches the lineRE, 
   or otherwise a null QString */
QString
Settings::testLine(const QString & line) const
{
  int pos;

  if ( (pos = lineRE.search(line, 0)) > -1)
    return QRegExp(lineRE).cap(0);
  return QString();
}

QString
Settings::testSectionLine(const QString & line) const
{
  if ( sectionRE.search(line, 0) > -1)
    return QRegExp(sectionRE).cap(1); // need to avoid breaking const correctness...
  return QString();
}

void
Settings::parseMatchedLine(const QString & matchedLine, 
                           QString & key, QString & value) const
{
  QRegExp lineRE_copy(lineRE); // need to avoid breaking const correctness...

  lineRE_copy.search(matchedLine, 0);
  key = lineRE_copy.cap(1);
  key.replace(QRegExp("^\\s*"), "");
  key.replace(QRegExp("\\s*$"), "");

  key.stripWhiteSpace();
  value = lineRE_copy.cap(2);
  value.replace(QRegExp("^\\s*"), "");
  value.replace(QRegExp("\\s*$"), "");
}


