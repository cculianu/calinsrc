#ifndef _SAMPLE_SOURCE_H
#define _SAMPLE_SOURCE_H

#include <string>
#include "shared_stuff.h"
#include "common.h"

# define SAMPLE_SOURCE_BLOCK_SZ_BYTES SS_RT_QUEUE_BLOCK_SZ_BYTES

class SampleStructSource {

 public:
  virtual ~SampleStructSource();
  virtual size_t numBytesReady() const = 0; // throws device exception
  virtual int numSamplesReady() const = 0; // is this really useful?
  virtual const SampleStruct * read(int b_time=-1) = 0; // throws eof, others  
  virtual void flush() = 0; /* flushes the reader input buffer */
  virtual size_t numBytesLastRead() const;
  virtual uint numSamplesLastRead() const;

  /* time in ms that this source suggests the reading process should
     spend waiting for more data, for maximal efficiency */
  virtual int suggestPollWaitTime() const { return 1; };

 protected:
  SampleStructSource(); /* abstract class.. no public constructors */
  size_t num_bytes_last_read, read_memory_sz;
  SampleStruct *read_memory; /* dyn. alloc array of SampleStructs 
                                from last read */
};

class SampleStructFileSource : public SampleStructSource {

 public:
  SampleStructFileSource(const string & filename);
  SampleStructFileSource(unsigned int filedes);
  virtual ~SampleStructFileSource();

  virtual size_t numBytesReady() const; // throws device exception
  virtual int numSamplesReady() const; // is this really useful?
  virtual const SampleStruct * read(int b_time = -1); // throws eof, others  
  virtual void flush() { /* no meaning */ }

  /* negative seconds indicates infinite waiting time */
  virtual void waitForNewData(int num_secs_to_wait = -1);  // throws eof

 protected:
  SampleStructFileSource() : fd(0) {} /* do not instantiate explicitly */
  virtual void open(const string & filename); // throws some exception
  virtual void close(); 
  
  int fd;
 
 private:
  bool i_opened_fd;
 
};

/* 
   DESIRED_FIFO_FEEL_MS --

   Defines the amount of time in milliseconds that we desire to wait
   on the RTF fifos (for 'feel' -- waiting too long leads to
   jerky graph drawing and the possibility of dropped samples) 
   The idea is that we try to sleep for around this time, if possible.
   However, we can't guarantee that we will sleep this time, because
   we also must take into account fifo size, sampling rate, and 
   daq system's own processing time.  

   What ends up happening is that we usually sleep for less than this time, 
   but that overall, daq_system's data sampling loop is periodic with a period
   of roughly DESIRED_FIFO_FEEL_MS.

*/
#define DESIRED_FIFO_FEEL_MS 30

class SampleStructFIFOSource : public SampleStructFileSource {
 public:
  SampleStructFIFOSource(unsigned int filedes);
  SampleStructFIFOSource (const string & filename);
  virtual ~SampleStructFIFOSource();
  void flush(); // flush old/stale data that may be in fifo, throws exceptions
};

#endif
