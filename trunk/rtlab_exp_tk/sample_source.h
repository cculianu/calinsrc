#ifndef _SAMPLE_SOURCE_H
#define _SAMPLE_SOURCE_H

#include <string>
#include "shared_stuff.h"

class SampleStructSource {

 public:
  virtual ~SampleStructSource();
  virtual size_t numBytesReady() const = 0; // throws device exception
  virtual int numSamplesReady() const = 0; // is this really useful?
  virtual const SampleStruct * read(int b_time=-1) = 0; // throws eof, others  
  virtual void flush() = 0; /* flushes the reader input buffer */
  virtual size_t numBytesLastRead() const;
  virtual unsigned int numSamplesLastRead() const;

 protected:
  SampleStructSource(); /* abstract class.. no public constructors */
  size_t num_bytes_last_read, max_read_memory;
  SampleStruct *read_memory; /* dyn. alloc array of SampleStructs 
				from last read */
};

class SampleStructFileSource : public SampleStructSource {

 public:
  SampleStructFileSource(const string & filename);
  virtual ~SampleStructFileSource();

  virtual size_t numBytesReady() const; // throws device exception
  virtual int numSamplesReady() const; // is this really useful?
  virtual const SampleStruct * read(int b_time = -1); // throws eof, others  

  /* negative seconds indicates infinite waiting time */
  virtual void waitForNewData(int num_secs_to_wait = -1);  // throws eof

 protected:
  SampleStructFileSource() : fd(0) {} /* do not instantiate explicitly */
  virtual void init(const string & filename); // calls open()
  virtual void destruct(); // calls close()
  virtual void open(const string & filename); // throws some exception
  virtual void close(); 
  
  int fd;

};

class SampleStructRTFSource : public SampleStructFileSource {
 public:
  SampleStructRTFSource(unsigned short minor = 0);
  void flush(); // flush old/stale data that may be in fifo, throws exceptions
  
};

#endif
