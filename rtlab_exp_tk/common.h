/*
 * This file is part of the RT-Linux Multichannel Data Acquisition System
 *
 * Copyright (C) 1999,2000 David Christini
 * Copyright (c) 2001 David Christini, Lyuba Golub, Calin Culianu
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

#ifndef _COMMON_H
#  define _COMMON_H
#  include <string>
#  include <string.h>
#  include "config.h"
#  include <values.h>
#ifndef BUG
#  include <stdio.h>
#define BUG() do { fprintf(stderr, "BUG at %s:%d!\n", __FILE__, __LINE__); } while (0)
#endif

#include "rtlab_types.h"

#define RTLAB_MODULE_NAME "rtlab"

#ifdef __cplusplus
#include <string>
#include <qstring.h>

using namespace std;


bool cstr_to_uint64(const char *in, uint64 & out);
uint64 cstr_to_uint64(const char *in, bool *ok = 0);

/* this function is non-reentrant! */
const char *uint64_to_cstr(uint64 in);


/* this function is non-reentrant! */
string operator+(const string & s, uint64 in);

/* this function is non-reentrant! */
QString operator+(const QString & s, uint64 in);


/* This is a syntactic-sugar class to support conversions to and from various 
   types  -- to use it just do:
       var_of_type2 = (Convert)var_of_incompatible_type1;

    Ideally we wanted operator=, but since C++ requires that be a member
    of the class in question, we can't use it.
*/
class Convert 
{
 private:
  QString qs;

  /* std::string ---> QString */
 public:
  explicit Convert(const std::string & s) : qs(s.c_str())  { }
  operator QString() const { return qs; }

  /* QString    ---> std::string */
 public:
  explicit Convert(const QString & qs) : qs(qs) { }
  operator std::string() const { return std::string(qs.latin1()); }

  /* uint64     ---> const char * */
 public:
  explicit Convert(uint64 n) { qs = uint64_to_cstr(n); }
  operator const char *() const { return qs.latin1(); }

  /* const char * ---> uint64 */
 public:
  explicit Convert(const char *cstr) : qs(cstr) {}
  operator uint64() const { return cstr_to_uint64(qs.latin1()); }

  /* double    ---> const char * */
 public:
  explicit Convert(double d) { qs.setNum(d); };  

  const char *cStr() const { return *this; }
  QString qStr() const { return qs; }
  std::string sStr() const { return cStr(); }
};

/* Checks whether file can be opened for reading ('r') or writing ('w'). 
   Returns an empty sting if opened successfully, 
   an error message otherwise. */
string fileErrorMsg ( const char *file_name, char mode ); 


#include <qstringlist.h>
/* For a given filename and a set of possible extensions, 
   returns filename with a new suffix/extension set to newSuffix.

   If the filename lacks the extention, returns filename + newSuffix */
QString forceFilenameExt(QString filename, 
                         QStringList suffixSet, 
                         QString newSuffix);

/* Just like the above function but provided for convenience */
QString forceFilenameExt(QString filename, 
                         QString suffix, 
                         QString newSuffix);

/* Simply removes whatever filename extention filename 
   may or may not have and replaces it with newSuffix */
QString forceFilenameExt(QString filename, QString newSuffix);
#endif

#endif
