/***************************************************************************
                          dsdstream.cpp  -  Daq System Data Stream
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
#ifdef DEBUG
#include <string>
#include <iostream>
#endif

#include <ieee754.h>

#include <qfile.h>
#include <qbuffer.h>
#include <qcstring.h>

#include <set>
#include <algorithm>

#include "dsdstream.h"

const uint DSDStream::MAGIC;

DSDStream::DSDStream() : QDataStream(), data_buf(0), data_buf_sz(0)
{
  init(0);
}

void DSDStream::init(int mode)// throw (FileException)
{
  currentIndex = lastIndex = 0;

  if (device()) {
    device()->open(mode);
    Assert<FileException>(isOpen(),
           "Internal Error: QIODevice error for DSDStream.", "IO Device could not be opened!");
  }

  resetScanFlags();
  alreadyBegan = false;
  history.clear();
  maskState.clear();
  rateState.clear();
  removeChannelQueue.clear();
}

void DSDStream::init(const QString & outFile, sampling_rate_t rate, FileDataType dataType)// throw (FileException)
{
  unsetDevice();
  setDevice(new QFile(outFile));
  init (IO_WriteOnly | IO_Truncate);
  rateState.rate = rate;
  rateChangedThisScan = true;
  fileDataType = dataType;
}

void DSDStream::init(const QString & inFile) //throw (FileException)
{
  unsetDevice();
  setDevice(new QFile(inFile));
  init (IO_ReadOnly);
}

DSDStream::~DSDStream()
{
  end();
}

void DSDStream::start() //throw (FileException, FileFormatException, IllegalStateException)
{
  if (alreadyBegan) return;

  Assert<IllegalStateException>(device(), "Illgal DSDStream Class State", "Cannot call start() without calling setOutFile() or setInFilere()");
  alreadyBegan = true;
  if (mode() & IO_WriteOnly) {
    *this << MAGIC << (uint)fileDataType;
    history.scanCount = 0;
    sampleData.clear();
    time(&history.timeStarted);
  } else if (mode() & IO_ReadOnly) {
    sampleData.clear();
    uint magic = 0, fdt = 0;
    *this >> magic >> fdt;
#ifdef DEBUG
    cerr << "Read magic: " << magic << " Fdt: " << fdt << endl;
#endif
    Assert<FileFormatException>( magic == MAGIC, "Bad file format", "This file is not a DSD file.");
    Assert<FileFormatException>( (fileDataType = uint2fdt(fdt)) != UNKNOWN_DATA_TYPE, "Unknown file data type",
                                "Unknown file data type encountered.  Currently only double and float data types are supported." );
    unserializeHistory();
  }
}

void DSDStream::end()
{
  if (isOpen() && (mode() & IO_WriteOnly)) {
    try {
      flush();
      maskState.endIndex = scanIndex();
      rateState.endIndex = scanIndex();
      history.endIndex = scanIndex();
      history.maskStates.push_back(maskState);
      history.rateStates.push_back(rateState);
      serializeHistory();
    } catch (Exception & e) {  }
  }
  unsetDevice(); // also deletes .. :)
  if (data_buf) { delete data_buf; data_buf = 0; data_buf_sz = 0; }
}

bool DSDStream::isChanOn(uint chan, scan_index_t atIndex) const
{
    if (maskState.startIndex <= atIndex && maskState.endIndex >= atIndex) return isChanOn(chan);
    return history.isChanOn(chan, atIndex);
}

/* this method is quite slow */
vector<uint> DSDStream::channelsOn(scan_index_t from, scan_index_t to) const
{
  vector<uint> ret;
  MaskState temp;

  if (from < history.startIndex) from = history.startIndex;
  if (to > history.endIndex) to = history.endIndex;

  /* is it current? */
  if ( from >= maskState.startIndex && to <= maskState.endIndex ) {
    temp = maskState;
    temp.computeChannelsOn();
    return temp.channels_on;
  }

  /* does it contain the current range? */
  if (to <= maskState.endIndex) {
    temp = maskState;
    temp.computeChannelsOn();
    ret = temp.channels_on;
  }

  /* it's a historical range.. so grab the relevant mask states and proceed..  */
  vector<MaskState>::const_iterator    it_from = history.maskStateAt(from),
                                       it_to   = history.maskStateAt(to),
                                       i;
  if ( it_from == history.maskStates.end() || it_to == history.maskStates.end() ) return ret;

  vector<uint>::const_iterator j;

  for (i = it_from; i <= it_to; i++) {
    temp = *i;
    temp.computeChannelsOn();
    for (j = temp.channels_on.begin(); j < temp.channels_on.end(); j++)
      if (::find(ret.begin(), ret.end(), *j) == ret.end())
        ret.push_back(*j);
  }

  ::sort(ret.begin(), ret.end());

  return ret;
}

scan_index_t DSDStream::scanCount(scan_index_t from, scan_index_t to) const
{
  scan_index_t ret = 0;

  if (from < history.startIndex) from = history.startIndex;
  if (to > history.endIndex) to = history.endIndex;

  ret = to - from + 1; // inclusive range

  // now compensate for dropped scans
  for (uint i = 0; i < history.skippedRanges.size(); i+=2) {
    uint skipped_from = history.skippedRanges[i];
    uint skipped_to = history.skippedRanges[i+1];

    if (skipped_to > to && skipped_from <= to) skipped_to = to;
    if (skipped_from < from && skipped_to >= from) skipped_from = from;

    if (from <= skipped_from && to >= skipped_to) {
      ret -= (skipped_to - skipped_from + 1);
    }
  }
  return ret;
}

sampling_rate_t DSDStream::rateAt(scan_index_t index) const
{
  if (index > history.endIndex || index < history.startIndex) return 0;
  for (vector<RateState>::const_iterator it = history.rateStates.begin(); it < history.rateStates.end(); it++) {
    if (it->startIndex <= index && it->endIndex >= index) return it->rate;
  }
  return 0;
}

map<scan_index_t, sampling_rate_t> DSDStream::ratesBetween (scan_index_t from, scan_index_t to) const
{
  map<scan_index_t, sampling_rate_t> ret;

  if (from < history.startIndex) from = history.startIndex;
  if (to > history.endIndex) to = history.endIndex;
  ret[from] = rateAt(from);
  for (vector<RateState>::const_iterator it = history.rateStates.begin(); it < history.rateStates.end(); it++) {
    if (it->startIndex <= from && it->endIndex >= from) continue; // we already have this one from the rateAt() call above
    if (it->startIndex <= to && it->endIndex > from)  ret[it->startIndex] = it->rate;
  }
  return ret;
}

double DSDStream::timeAt (scan_index_t index) const
{
  double ret = timeStarted();

  if (index < history.startIndex || index > history.endIndex) return 0;

  map<scan_index_t, sampling_rate_t> rates = ratesBetween(history.startIndex, index);
  //sort(rates.begin(), rates.end());
  map<scan_index_t, sampling_rate_t>::iterator it, next;
  for (next = it = rates.begin(); it != rates.end(); next = ++it) {
    next++;
    scan_index_t start = it->first, end = (next != rates.end() ? next->first - 1 : index);
    ret += (double) (end - start + 1)/((double)it->second);
  }
  return ret;
}

double DSDStream::wcTimeAt (scan_index_t index) const
{
  double t = timeAt(index);
  if (t == 0) return 0;
  return ((double)timeStarted()) + timeAt(index);
}

void DSDStream::setSamplingRate ( sampling_rate_t r  )
{
  history.rateStates.push_back( rateState );
  rateState.startIndex = ++rateState.endIndex;
  rateState.rate = r;
  rateChangedThisScan = true;
}

void DSDStream::flush() //throw (FileException)
{
  switch(fileDataType) {
  case DOUBLE:
    flushTempl<double>();
    break;
  case FLOAT:
    flushTempl<float>();
    break;
  default:
    throw FileFormatException("INTERNAL ERROR", "Unknown file data type specified");
    break;
  }
}

template<class T> void DSDStream::flushTempl() //throw (FileException)
{
  if (!(mode() & IO_WriteOnly) || !isOpen() || !flushPending) return;

  if (chanMaskChangedThisScan)
    putMaskChangedInsn();
  if (rateChangedThisScan)
    putRateChangedInsn();
  if (sampleSkippedThisScan)
    putIndexChangedInsn();


  for (vector<double>::iterator it = sampleData.begin(); it < sampleData.end(); it++) {
    *this << (T)(*it);
    history.sampleCount++;
  }

  resetScanFlags();
  lastIndex = scanIndex();
}

void DSDStream::writeSample ( const SampleStruct * s ) //throw (IllegalStateException, FileException)
{
  if (!(mode() & (IO_WriteOnly | IO_Truncate)) ) return;

  if (!alreadyBegan) { start(); history.scanCount++;/* hack */ }

  Assert<IllegalStateException>(s->scan_index >= scanIndex(),
                               "Illegal DSDStream state.", "Sample encountered out of order!");

  if (s->scan_index > scanIndex()) {
    flush();
    history.scanCount++;
    setScanIndex (s->scan_index);
  }


  if (!maskState.mask.isOn(s->channel_id)) {
    addChannel(s->channel_id);
  }

  sampleData[maskState.id_to_pos_map[s->channel_id]] = s->data;
  chans_this_scan++;
  flushPending = true;
}

// this seems to be broken in that it always returns true!
template<class T> bool DSDStream::readNextSampleTempl ( SampleStruct * s) //throw (FileException)
{
  bool have_insn = false;

  get_it_from_cache:
  if (chans_this_scan) {
    uint chan = maskState.channels_on[maskState.mask.numOn()-chans_this_scan];
    s->scan_index = currentIndex;
    s->channel_id = chan;
    s->data = sampleData[maskState.id_to_pos_map[chan]];
    chans_this_scan--;
    if (! chans_this_scan)  maskState.endIndex = rateState.endIndex = ++currentIndex;
    return true;
  }

  grab_insn:

  if (scanIndex() > history.endIndex) return false;

  chkDataBuf(sizeof(T));

  *this >> *(T *)data_buf;
  if (device()->status() != IO_Ok) return false;

  have_insn = isNaN((T *)data_buf);

  if (have_insn)  {
    doInsn();
    goto grab_insn; // this keeps consuming the instructions until we reach a real data element
  }

  if (!maskState.mask.numOn()) goto grab_insn; // keep consuming instructions

  uint loopTimes = maskState.mask.numOn();

  for(chans_this_scan = 0; chans_this_scan < loopTimes; chans_this_scan++) {
    if ( chans_this_scan > 0 ) {
      *this >> *((T *)data_buf);
      if (device()->status() != IO_Ok) break;
    }
    sampleData[chans_this_scan] = *((T *)data_buf);
  }

  if (chans_this_scan)  goto get_it_from_cache;
  return false;
}

/* reads the next full scan and modifies m to be channel-id -> SampleStruct  */
bool
DSDStream::readNextScan (map<uint, SampleStruct> & m) //throw (IllegalStateException, FileFormatException, FileException)
{
  bool ret = true;
  uint i;
  SampleStruct s;

  m.clear();

  if (chans_this_scan == 0)  {
    ret = readNextSample(s);
    m[s.channel_id] = s;
  }

  for (i = 0; chans_this_scan && ( ret = readNextSample(s) ) ; i++)
    m[s.channel_id] = s;

  return ret;
}

bool DSDStream::readNextSample ( SampleStruct * s ) //throw (IllegalStateException, FileFormatException, FileException)
{
  if (!(mode() & (IO_ReadOnly)) ) return false;

  if (!alreadyBegan) start(); // returns very quickly if this is called again after the first time

  switch(fileDataType) {
  case DOUBLE:
    return readNextSampleTempl<double>(s);
    break;
  case FLOAT:
    return readNextSampleTempl<float>(s);
    break;
  default:
    throw FileFormatException("INTERNAL ERROR", "Unknown file data type specified");
    break;
  }
}

void DSDStream::resetScanFlags() { flushPending = sampleSkippedThisScan = rateChangedThisScan = chanMaskChangedThisScan = false; chans_this_scan = 0; }

void DSDStream::maskChanged()
{
  vector<double> tempData = sampleData;
  vector<uint> tempchanson = maskState.channels_on;
  maskState.computeChannelsOn();
  sampleData.resize(maskState.mask.numOn());
  for (uint i = 0; i < tempData.size(); i++) {
    if (maskState.id_to_pos_map.find(tempchanson[i]) != maskState.id_to_pos_map.end())
      sampleData[maskState.id_to_pos_map[tempchanson[i]]] = tempData[i];
  }
}

void DSDStream::addChannel(uint id)
{
  if (!chanMaskChangedThisScan) {
    maskState.endIndex = lastIndex;
    history.maskStates.push_back(maskState);
    maskState.endIndex = maskState.startIndex = scanIndex();
    chanMaskChangedThisScan = true;
  }
  maskState.mask.setOn(id, true);
  maskChanged();
}

void DSDStream::removeChannelAfter(uint id, scan_index_t future_scan)
{
  if (future_scan <= scanIndex()) return;
  removeChannelQueue[future_scan].insert(id);
}

void DSDStream::removeChannel(uint id)
{
  if (!chanMaskChangedThisScan) {
    maskState.endIndex = lastIndex;
    history.maskStates.push_back(maskState);
    maskState.endIndex = maskState.startIndex = scanIndex();
    chanMaskChangedThisScan = true;
  }
  maskState.mask.setOn(id, false);
  maskChanged();
}


struct my_pred {
  my_pred(scan_index_t b) : b(b) {};
  bool operator()(const pair<scan_index_t, set<uint> > & a) { return a.first < b; };
  const scan_index_t b;
};

// only call this from a writing device!
void DSDStream::setScanIndex(scan_index_t index)
{

  if (index > scanIndex()+1) {
    sampleSkippedThisScan = true;
    history.skippedRanges.push_back(scanIndex()+1);
    history.skippedRanges.push_back(index-1);
  }
  rateState.endIndex = maskState.endIndex = history.endIndex = currentIndex = index;
  // now, see if there are any pending channel removes in the queue and put the insn into in the stream if there are
  map<scan_index_t, set<uint> >::iterator item;
  vector<scan_index_t> tmpList; tmpList.push_back(scanIndex());
  while ( (item  = ::find_if(removeChannelQueue.begin(), removeChannelQueue.end(), my_pred(scanIndex())) )
          != removeChannelQueue.end() ) {
    for (set<uint>::iterator it = item->second.begin(); it != item->second.end(); it++)
      removeChannel(*it);
    removeChannelQueue.erase(item);
  }
}


void DSDStream::doInsn()
{
  uint64 tmpVal;
  uint tmpInsn = UNKNOWN_INSN;
  *this >> tmpInsn;
  switch ( uint2insn(tmpInsn) ) {
  case MASK_CHANGED_INSN:
    *this >> maskState.mask;
    maskState.startIndex = currentIndex;
    maskChanged();
#ifdef DEBUG
    cerr << (string("Found mask changed instruction in file. Changing mask for index ") + maskState.startIndex) << " total chans: "
         << maskState.mask.numOn() << endl;
#endif
    break;
  case RATE_CHANGED_INSN:
    *this >> rateState.rate;
    rateState.startIndex = currentIndex;
#ifdef DEBUG
    cerr << (string("Found rate changed instruction in file. Changing rate to ") + rateState.rate) << (string(" for index ") + rateState.startIndex) << endl;
#endif
    break;
  case INDEX_CHANGED_INSN:
     tmpVal = currentIndex;
    *this >> currentIndex;
#ifdef DEBUG
    cerr << ((string("Found index changed instruction in file. Changing index from ") + tmpVal) + (string(" to ") + currentIndex)) << endl;
#endif
    break;
  default:
    throw FileFormatException("INTERNAL ERROR", "Unknown instruction encountered in data file!");
    break;
  }
}

void DSDStream::putInsn(Instruction ins)
{
  chkDataBuf(sizeof(double));

  switch (fileDataType) {
  case DOUBLE:
      setNaN((double *)(data_buf));
      *this << *((double *)data_buf);
      break;
  default:
  case FLOAT:
      setNaN((float *)(data_buf));
      *this << *((float *)data_buf);
      break;
  }

  *this << (Q_UINT32)ins;
}

void DSDStream::putMaskChangedInsn()
{
  static const Instruction instruction = MASK_CHANGED_INSN;

  putInsn(instruction);
  *this << maskState.mask;
}

void DSDStream::putRateChangedInsn()
{
  static const Instruction ins = RATE_CHANGED_INSN;

  putInsn(ins);
  *this << rateState.rate;
}

void DSDStream::putIndexChangedInsn()
{
  static const Instruction ins = INDEX_CHANGED_INSN;

  putInsn(ins);
  *this << scanIndex();
}


QDataStream & DSDStream::writeRawBytes (const char * s, uint len) //throw (FileException)
{
      device()->resetStatus();
      QDataStream & ret = QDataStream::writeRawBytes(s, len);

      chkIOErr(device());

      return ret;
}

QDataStream & DSDStream::readRawBytes ( char * s, uint len ) //throw (FileException)
{
      device()->resetStatus();
      QDataStream & ret = QDataStream::readRawBytes(s, len);

      chkIOErr(device());

      return ret;
}

// only should be called from within end()!
void DSDStream::serializeHistory() //throw (FileException)
{
  QByteArray byte_arr;
  QBuffer buf(byte_arr);
  Settings settings(&buf);


  history.computeMaxUniqueChannelsUsed();
  history.serialize(settings, "State History Metadata");
  settings.saveSettings();
  footerLength = byte_arr.size();

  *this << byte_arr << footerLength << MAGIC;
  device()->flush();
  footerOffset = device()->size() - (footerLength+sizeof(footerLength)+sizeof(MAGIC));
#ifdef DEBUG
  cerr << "Footer offset: " << footerOffset << " Footer Length: " << footerLength << " File size: " << device()->size() << endl;
#endif
}

void DSDStream::unserializeHistory() //throw (FileException)
{
  QIODevice::Offset whereWeBegan = device()->at();

  device()->at(device()->size() - ( sizeof(footerLength)+sizeof(MAGIC) ) );
  uint magic = 0;
  *this >> footerLength >> magic;
  Assert<FileFormatException>( MAGIC == magic,
                               "File format bad",
                               "This file was not properly closed and is missing its metadata! Run the DSD repair tool!");
  footerOffset = device()->size() - (footerLength+sizeof(footerLength)+sizeof(MAGIC));
#ifdef DEBUG
  cerr << "Footer offset: " << footerOffset << " Footer Length: " << footerLength << " File size: " << device()->size() << endl;
#endif
  device()->at(footerOffset);

  QByteArray byte_arr;
  byte_arr.resize(footerLength);
  device()->readBlock(byte_arr.data(), byte_arr.size());
  QBuffer buf(byte_arr);
  Settings settings(&buf);
  settings.parseSettings();

  history.unserialize(settings, "State History Metadata");
  device()->at(whereWeBegan);
}

inline uint DSDStream::getExponentNaN(const float * dummy) const { dummy++; return 255; }
inline uint DSDStream::getExponentNaN(const double * dummy) const { dummy++; return 2047; }

#define __isNaN_TEMPL(TYPE, STRUCT_TYPE) \
inline bool DSDStream::isNaN (const TYPE * in) const \
{ \
  const STRUCT_TYPE *f = (const STRUCT_TYPE *)in; \
  return (f->ieee_nan.exponent == getExponentNaN(in) && f->ieee_nan.quiet_nan); \
}

__isNaN_TEMPL(float, ieee754_float)
__isNaN_TEMPL(double, ieee754_double)

#undef __isNaN_TEMPL

#define __setNaN_TEMPL(TYPE, STRUCT_TYPE, DATA_ELEMENT) \
inline void DSDStream::setNaN (TYPE * fp) const \
{ \
  STRUCT_TYPE *nan = (STRUCT_TYPE *)fp; \
\
  nan->DATA_ELEMENT = 0; \
\
  nan->ieee_nan.quiet_nan = 1; nan->ieee_nan.exponent = getExponentNaN(fp); \
}

__setNaN_TEMPL(float, ieee754_float,f)
__setNaN_TEMPL(double, ieee754_double,d)
#undef __setNaN_TEMPL

template<> DSDStream & DSDStream::operator>>(char & t) //throw(FileException)
{
  Q_INT8 q; // prevents some compiler warnings...
  *this >> q;
  t = q;
  return *this;
};

/* the following specialization is pretty platform-specific -- todo: revice this for 64-bit arch's */
template<> DSDStream & DSDStream::operator>>(uint64 & t) //throw(FileException)
{
  Q_UINT8 * a = reinterpret_cast<Q_UINT8 *>(&t);
  uint i;
  for (i = 0; i < sizeof(uint64); i++)
    *this >> a[i];
  return *this;
};
template<> DSDStream & DSDStream::operator>>(int64 & t) //throw(FileException)
{
  *this >> *reinterpret_cast<uint64 *>(&t);
  return *this;
};

template<> DSDStream & DSDStream::operator<<(const char & t) //throw (FileException)
{
  *this<<((Q_UINT8)t);
  return *this;
};

template<> DSDStream & DSDStream::operator<<(const uint64 & t) //throw (FileException)
{
  const Q_UINT8 *a = reinterpret_cast<const Q_UINT8 *>(&t);
  uint i;
  for (i = 0; i < sizeof(uint64); i++)
    *this << a[i];
  return *this;
};

template<> DSDStream & DSDStream::operator<<(const int64 & t) //throw (FileException)
{
  *this << *reinterpret_cast<const uint64 *>(&t);
  return *this;
};


