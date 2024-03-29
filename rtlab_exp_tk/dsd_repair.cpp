#include <iostream>
#include "dsd_repair.h"

using namespace std;

void DSDRStream::start()
{
  try {    

    sampleData.clear();
    meta_data.clear();
    uint magic = 0, fdt = 0;
    *this >> magic >> fdt;
#ifdef DEBUGGG
    cerr << "Read magic: " << magic << " Fdt: " << fdt << endl;
#endif
    Assert<FileFormatException>( magic == MAGIC, "Bad file format", "This file is not a DSD file.");
    Assert<FileFormatException>( (fileDataType = uint2fdt(fdt)) != UNKNOWN_DATA_TYPE, "Unknown file data type",
                                "Unknown file data type encountered.  Currently only double and float data types are supported." );
    
    throw FileFormatException();
    //DSDStream::start();

  } catch (FileFormatException & e) {
    no_meta_data = true;
    uint magic = 0, fdt = 0;
    device()->at(0);
    *this >> magic >> fdt;
    Assert<FileFormatException>( magic == MAGIC, "Bad file format", 
                                 "This file is not a DSD file.");
    Assert<FileFormatException>( (fileDataType = uint2fdt(fdt)) != UNKNOWN_DATA_TYPE, "Unknown file data type", "Unknown file data type encountered.  Currently only double and float data types are supported." );    
    createMetaData();
    alreadyBegan = true;
  }
}

void DSDRStream::createMetaData()
{
  history.endIndex = (uint64)-1;
}


/* reads the next full scan and modifies m to be channel-id -> SampleStruct  */
bool
DSDRStream::readNextScan (map<uint, SampleStruct> & m) //throw (IllegalStateException, FileFormatException, FileException)
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


/* reads the next full scan and modifies v  */
bool
DSDRStream::readNextScan (vector<SampleStruct> & v) //throw (IllegalStateException, FileFormatException, FileException)
{
  v.clear();
  bool ret = true;
  uint i;
  SampleStruct s;

  v.clear();

  if (chans_this_scan == 0)  {
    ret = readNextSample(s);
    v.push_back(s);
  }

  for (i = 0; chans_this_scan && ( ret = readNextSample(s) ) ; i++)
    v.push_back(s);

  return ret;
}


bool DSDRStream::readNextSample ( SampleStruct * s ) 
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





template<class T> bool DSDRStream::readNextSampleTempl ( SampleStruct * s) //throw (FileException)
{
  bool have_insn = false;

  get_it_from_cache:
  if (chans_this_scan) {
    uint chan = maskState.channels_on[maskState.mask.numOn()-chans_this_scan];
    s->scan_index = currentIndex;
    s->channel_id = chan;
    s->data = sampleData[maskState.id_to_pos_map[chan]];
    s->spike = 0; /* spike information in datafiles not yet supported! */
    s->magic_number = SAMPLE_STRUCT_MAGIC;
    chans_this_scan--;
    if (! chans_this_scan)  {
      maskState.endIndex = rateState.endIndex = ++currentIndex;
      user_data.clear();
    }
    return true;
  }

  grab_insn:

  if (!no_meta_data && scanIndex() > history.endIndex) return false;

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

