/***************************************************************************
                          dsd_repair.h  -  Daq System Data Stream Repair Clarr
                             -------------------
    begin                : Mon Oct 28 2002
    copyright            : (C) 2002 by Calin Culianu
    email                : calin@rtlab.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#include "dsdstream.h"

using namespace std;

class DSDRStream : public DSDIStream {
public:

  DSDRStream()  { init(); }
  DSDRStream (const QString & inFile) : DSDIStream(inFile)
    { init();  } ;
  
  void start();
  bool readNextScan (map<uint, SampleStruct> & m);
  bool readNextScan (vector<SampleStruct> & v);

  bool readNextSample ( SampleStruct * s );
  bool readNextSample ( SampleStruct & s ) { return readNextSample(&s); };

private:
  bool no_meta_data;

  void createMetaData();

  void init() { no_meta_data = false; }

  template<class T> bool readNextSampleTempl ( SampleStruct * s);

protected:

   template<class T> DSDStream & operator>>(T & t) /*throw (FileException)*/ {
      *static_cast<QDataStream *>(this) >> (t);
      return *this;
   };

};
