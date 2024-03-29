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
#include <vector>
#include <qiodevice.h>
#include <qstring.h>

/* 
   todo: figure out a nice way to use hashsets on  all this so
   that this class can be configured more easily 

   also: maybe make this class automatically read from
         ConfigurationWindow so that it can automatically update itself??
*/


class Settings
{
 public:
  typedef map<QString, QString> Section;
  typedef set<QString> DirtyKeys;  
  typedef map<QString, DirtyKeys > DirtyMap;
  typedef map<QString, Section> SettingsMap;

  Settings();
  Settings(QIODevice * device);

  virtual ~Settings();

  virtual void parseSettings(); // NB: this clobbers the existing settings in this class!
  virtual void saveSettings();
  virtual QString getConfigFileName() const; // automatically creates a QFile
  virtual void setConfigFileName(const QString & name); /*  after setting this,
                                                            typically you need
                                                            to call
                                                            parseConfig() */

  // alternate usage, pass in a QIODevice
  virtual void setConfigDevice(QIODevice * device);
  virtual QIODevice * getConfigDevice() const;

  virtual QIODevice::Offset length() const;

  virtual QString currentSection() const { return _currentSection; };
  virtual void setSection(const QString & section) { _currentSection = section; };

  virtual QString get(const QString & key) const;
  virtual void put(const QString &key, const QString &value);

  virtual QString get(const QString & section, const QString & key) const;
  virtual void put(const QString & section, const QString &key, const QString &value);

  virtual void putSection(const QString & section_name, const Section & section);
  virtual void mergeSection(const QString & section_name, const Section & section);
  virtual Section getSection(const QString & section_name) const;

  /* retrieves all the section names available in this settings instance */
  virtual vector<QString> sections() const;

  /* two identical methods to merge one settings instance into this one */
  Settings & operator<<(const Settings & merge_from);
  virtual Settings & merge(const Settings & merge_from);

 protected:

  QString _currentSection;

  DirtyMap dirtySettings;

  SettingsMap settingsMap;

  /* a derived class may optionally set masterSettings -- without it all settings in the file are read and acknowledged */
  const SettingsMap *masterSettings;

/* reads all the name/value pairs in the settings file and throws them in a map and returns that */
  virtual SettingsMap readAll();

  /* parses a VALID settings line and populates key and value */
  virtual void parseMatchedLine(QString matchedline, 
                                QString & key, QString & value) const;

  /*
    returns a matched substring from line if line matches the perl-like re:
        \s*\S+\s*=\s*\S+

     otherwise returns QString::null.
  */
  virtual QString testLine(QString line) const;
  /* 
     returns the section name if the line matches the form:
        \s*\[\S*\]\s*
     otherwise returns null
 */
  virtual QString testSectionLine(QString line) const;

  virtual void maybeDeleteConfigFile(); // kind of awkward but necessary

  QIODevice *configFile;
  bool need_to_delete_configFile;

 private:
  void init();
};




#endif
