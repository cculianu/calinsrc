/***************************************************************************
                          dsdstream.h  -  Daq System Data Stream
                             -------------------
    begin                : Thu Feb 7 2002
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

#ifndef DSDSTREAM_H
#define DSDSTREAM_H

#include <vector>
#include <map>
#include <set>

#include <qstring.h>
#include <qdatastream.h>
#include <qbitarray.h>
#include <qbuffer.h>


#include <string.h>
#include <time.h>

#include "common.h"
#include "exception.h"
#include "shared_stuff.h"
#include "settings.h"

/**
  *@author Calin Culianu
  */

inline void chkIOErr(const QIODevice * d, const char *msg1 = 0, const char *msg2 = 0) throw (FileException)
{
    Assert(d, "INTERNAL ERROR", "chkIOErr() was passed a null device!");
    if (msg1 && msg2)
      Assert<FileException>(d->status() == IO_Ok, msg1, msg2);
    else if (d->mode() & IO_WriteOnly)
      Assert<FileException>(d->status() == IO_Ok, "IO Error writing to the output file", "An IO error occurred while writing to the output file");
    else if (d->mode() & IO_ReadOnly)
      Assert<FileException>(d->status() == IO_Ok, "IO Error reading from the input file", "An IO error occurred while reading from the input file");
}

class DSDStream : protected QDataStream  {

#define _INSIDE_DSDSTREAM
#include "dsdstream_inner.h"
#undef _INSIDE_DSDSTREAM

  friend struct Serializeable;
  friend struct ChannelMask;
  friend struct MaskState;
  friend struct RateState;
  friend struct StateHistory;

public:
  enum FileDataType {  FLOAT = 0,   DOUBLE,   UNKNOWN_DATA_TYPE };

protected:
  DSDStream();
public:

  ~DSDStream();

  /* call this to setup/compute some stats on a reading stream -- otherwise methods like scanCount() and sampleCount() will be
     innacurate.  Otherwise this is implicityly called by writeSample() and readNextSample() */
  void start() throw (FileException, FileFormatException, IllegalStateException);

  /* call this to close the output file and flush pending writes.  Writing no longer possible after this is called */
  /* on a reading stream, this closes the infile, invalidating all future reads */
  void end();


  int mode() const { if (device()) return device()->mode(); return 0; }; // returns the file mode for this device
  bool isOpen() const { if (device()) return device()->isOpen(); return false; };

  /* it is imperative that this be called after a full scan has been written but before a new one begins,
     because this increments the current scan index */
  void setSamplingRate ( sampling_rate_t r );
  sampling_rate_t samplingRate() const  { return rateState.rate; };

  scan_index_t scanIndex() const  { return currentIndex; };

  /* all of the following methods return undefined values if start() was never called on a reading
     stream */
  scan_index_t         scanCount() const { return history.scanCount; }; // number of scans in file, minus the ones skipped...
  scan_index_t         scanCount(scan_index_t from, scan_index_t to) const; // the number of scans in a certain inclusive range, minus the ones skipped
  uint64               sampleCount() const { return history.sampleCount; }; // the total number of samples in the file
  uint                 maxUniqueChannelsUsed() const { return history.max_unique_channels_used; };
  scan_index_t         startIndex() const  { return history.startIndex; };
  scan_index_t         endIndex() const { return history.endIndex; };
  bool                 isChanOn(uint chan) const { return maskState.mask.isOn(chan); };
  bool                 isChanOn(uint chan, scan_index_t atIndex) const;
  const vector<uint> & channelsOn() const { return maskState.channels_on; };
  vector<uint>         channelsOn(scan_index_t fromIndex, scan_index_t toIndex) const; /* this method is quite slow */
  time_t               timeStarted() const { return history.timeStarted; }; // the wall-clock-time that the first sample was written
  double               timeAt (scan_index_t index) const; // the relative time at this index, 0 if invalid index
  double               wcTimeAt (scan_index_t index) const; // the wall-clock-time at a particular index
  sampling_rate_t      rateAt (scan_index_t index) const;
  map<scan_index_t,
  sampling_rate_t>     ratesBetween (scan_index_t from, scan_index_t to) const; // inclusive range, with the (inlusive) scan_index that they started at
  /*
     Writes a sample to the device
     May throw an exception if the sample has an illegal scan index.
     This generally is a scan index that is lower than the current index.
     Does nothing if d->mode() is not (IO_WriteOnly | IO_Truncate)
  */
  void writeSample ( const SampleStruct * s) throw (IllegalStateException, FileFormatException, FileException);

  /*
     Reads a sample from the device
     Does nothing if d->mode() is not (IO_ReadOnly)
  */
  bool readNextSample (SampleStruct * s) throw (IllegalStateException, FileFormatException, FileException);

  bool readNextSample (SampleStruct & s) throw (IllegalStateException, FileFormatException, FileException)
    { return readNextSample(&s);}

  /* reads the next full scan and modifies m to be channel-id -> SampleStruct */
  bool readNextScan (map<uint, SampleStruct> & m) throw (IllegalStateException, FileFormatException, FileException);

  FileDataType dataType() const { return fileDataType; };

  /* can only set the data type if writing hasn't already begun */
  void setDataType(FileDataType t) { if (!alreadyBegan) fileDataType = t; };

  /* call this to inform the filewriter that AFTER furute_index, chan will no longer produce data
     if chan starts to produce data again after future_index, then it is not a fatal error, that
     channel will simply be auto-sensed to on */
  void removeChannelAfter(uint chan, scan_index_t future_index);


protected:
   QDataStream & writeRawBytes (const char * s, uint len) throw (FileException);

   QDataStream & readRawBytes (char * s, uint len) throw (FileException);

   template<class T> DSDStream & operator>>(T & t) throw (FileException) {
      *static_cast<QDataStream *>(this) >> (t);
      chkIOErr(device());
      return *this;
   };

   template<class T> DSDStream & operator<<(const T & t) throw (FileException) {
      *static_cast<QDataStream *>(this) << (t);
      chkIOErr(device());
      return *this;
   };

   template <class T> DSDStream & operator>>(vector<T> & v) throw (FileException) {
     size_t size;
     *this >> size;
     v.resize(size);
     for (uint i = 0; i < size; i++) *this >> v[i];
     return *this;
   };

   template <class T> DSDStream & operator<<(const vector<T> & v) throw (FileException) {
       *this << v.size(); const T * it;
       for (it = v.begin(); it != v.end(); it++)
         *this <<  *it;
       return *this;
   };


  void init(int mode) throw (FileException);
  void init(const QString & outFile, sampling_rate_t rate, FileDataType dataType) throw (FileException);
  void init(const QString & inFile) throw (FileException);
  void unsetDevice() {   QIODevice * d = device(); QDataStream::unsetDevice(); if (d) { d->close(); delete d; } };

private:

  uint getExponentNaN (const float * dummy) const;
  uint getExponentNaN (const double * dummy) const;

  bool isNaN (const float * d) const;
  bool isNaN (const double * d) const;

  void setNaN (float * d) const;
  void setNaN (double * d) const;

  template<class T> bool readNextSampleTempl ( SampleStruct * s) throw (FileException);
  template<class T> void flushTempl() throw (FileException); // it is important to call this before closing your QIODevice!
  void flush() throw(FileException); // calls above flush with the right template param

  void resetScanFlags();

  void maskChanged(); // called by addChannel() removeChannel() and doInsn()
  void addChannel(uint c);
  void removeChannel(uint c);

  // only call this from a writing stream!
  void setScanIndex(scan_index_t index);

  enum Instruction {
    MASK_CHANGED_INSN = 0x01,
    RATE_CHANGED_INSN = 0x02,
    INDEX_CHANGED_INSN = 0x03,
    UNKNOWN_INSN
  };
  void putInsn(Instruction ins);
  void putMaskChangedInsn();
  void putRateChangedInsn();
  void putIndexChangedInsn();
  void doInsn();

  void serializeHistory() throw (FileException);
  void unserializeHistory() throw (FileException);

  static Instruction uint2insn(uint i)
  {
    switch (i) {
    case MASK_CHANGED_INSN:
      return MASK_CHANGED_INSN; break;
    case RATE_CHANGED_INSN:
      return RATE_CHANGED_INSN; break;
    case INDEX_CHANGED_INSN:
      return INDEX_CHANGED_INSN; break;
    default:
      break;
    }
    return UNKNOWN_INSN;
  };

  static FileDataType uint2fdt(uint i)
  {
    switch(i) {
    case DOUBLE:
      return DOUBLE; break;
    case FLOAT:
      return FLOAT; break;
    default:
      break;
    }
    return UNKNOWN_DATA_TYPE;
  };

  StateHistory history;
  RateState rateState;
  MaskState maskState;
  FileDataType fileDataType;
  bool alreadyBegan, chanMaskChangedThisScan, rateChangedThisScan, sampleSkippedThisScan, flushPending;
  vector<double> sampleData;
  scan_index_t lastIndex, currentIndex; // the last index flushed to disk, and the current index being worked on
  static const uint MAGIC = 0xf117;
  size_t footerLength, /* length of the file footer, sans the trailing encoded footerLength and MAGIC data... */
         footerOffset; /* position in the file where the footer begins */

  uint chans_this_scan;

  map<scan_index_t, set<uint> > removeChannelQueue;

  char *data_buf;
  size_t data_buf_sz;

  void chkDataBuf(size_t size) {
    if (data_buf_sz < size ) {
      if (data_buf) {
        char *tmp = new char[data_buf_sz];
        for (uint i = 0; i < data_buf_sz; i++) tmp[i] = data_buf[i];
        delete data_buf;
        data_buf = new char[size];
        for (uint i = 0; i < data_buf_sz; i++) data_buf[i] = tmp[i];
        delete tmp;
      } else data_buf = new char[size];
      data_buf_sz = size;
    }
  }

};

class DSDIStream: public DSDStream {
public:
  /* reading DSDStream constructor */
  DSDIStream() : DSDStream() {};
  DSDIStream (const QString & inFile) throw (FileException)
    { setInFile(inFile); } ;
  void setInFile(const QString & inFile) throw (FileException)
    { end(); init(inFile); } ;
  void jumpToScanIndex(scan_index_t index) throw (IllegalStateException, FileFormatException, FileException)
    { index++;/* stub for now */ };

};

class DSDOStream: public DSDStream {
public:
  /* writing DSDStream constructor */
  DSDOStream() : DSDStream() {};
  DSDOStream( const QString & outFile, sampling_rate_t rate, FileDataType dataType = FLOAT) throw (FileException)
   { setOutFile(outFile, rate, dataType); };
  void setOutFile( const QString & outFile, sampling_rate_t rate, FileDataType dataType = FLOAT  ) throw (FileException)
   { end(); init(outFile, rate, dataType); };
};

template<> DSDStream & DSDStream::operator>>(char & t) throw(FileException);
template<> DSDStream & DSDStream::operator>>(uint64 & t) throw(FileException);
template<> DSDStream & DSDStream::operator>>(int64 & t) throw(FileException);
template<> DSDStream & DSDStream::operator<<(const char & t) throw (FileException);
template<> DSDStream & DSDStream::operator<<(const uint64 & t) throw (FileException);
template<> DSDStream & DSDStream::operator<<(const int64 & t) throw (FileException);
template<> DSDStream & DSDStream::operator<<(const ChannelMask & m) throw (FileException); // look in dsdstream_inner.cpp for definition
template<> DSDStream & DSDStream::operator>>(ChannelMask & m) throw (FileException);       // ''

#endif
