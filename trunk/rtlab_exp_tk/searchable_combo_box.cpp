/*
 * This file is part of the RT-Linux Multichannel Data Acquisition System
 *
 * Copyright (C) 2002 David Christini, Calin Culianu
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
#include <qcombobox.h>
#include <qstring.h>
#include "searchable_combo_box.h"

SearchableComboBox::SearchableComboBox ( QWidget * parent, const char * name)
  : QComboBox ( parent, name ) {}
SearchableComboBox::SearchableComboBox ( bool rw, QWidget * parent, const char * name)
  : QComboBox ( rw, parent, name ) {}

int SearchableComboBox::findItem(const QString & str, int criteria)
{
  QString txt = (criteria & CaseSensitive ? str : str.lower()) ;
  
  for (int i = 0; i < count(); i++) {
    QString item = (criteria & CaseSensitive ? text(i) : text(i).lower());

    if (   
           criteria & Contains   && item.contains(txt) 
        || criteria & BeginsWith && item.startsWith(txt) 
        || criteria & EndsWith   && item.endsWith(txt)
        || criteria & ExactMatch && item == txt
       ) 
      return i;
  }
  return -1;
}
