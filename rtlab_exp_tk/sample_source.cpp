#include <string>
#include <strstream>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <string.h>
#include "exception.h"
#include "sample_source.h"
#include "shared_stuff.h"

/*----------------------------------------------------------------------------
  SampleStructSource method definitions
----------------------------------------------------------------------------*/

SampleStructSource::SampleStructSource() 
{ 
  read_memory = NULL;
  max_read_memory = num_bytes_last_read = 0;
}

SampleStructSource::~SampleStructSource()
{
  if (read_memory) delete read_memory;
}

size_t
SampleStructSource::numBytesLastRead() const
{
  return num_bytes_last_read; 
}

unsigned int 
SampleStructSource::numSamplesLastRead() const
{
  return numBytesLastRead() / sizeof (SampleStruct);
}

SampleStructFileSource::SampleStructFileSource (const string & filename)
{
  init(filename);
}

SampleStructFileSource::~SampleStructFileSource ()
{
  destruct();
}

void
SampleStructFileSource::waitForNewData(int num_secs = -1)
{
  struct timeval tv;
  fd_set rfds;
  
  tv.tv_usec = 0;
  tv.tv_sec = num_secs;
  FD_ZERO(&rfds);
  FD_SET(fd, &rfds);

  /* block num_secs time waiting for data, or 
     indefinitely if num_secs is less than zero */
  errno = 0;
  int ret = select (fd+1, &rfds, NULL, NULL, (num_secs < 0 ? NULL : &tv)); 

  if ( ret < 0 && errno != EINTR && errno != ENOMEM && numBytesReady() == 0 ) {
    throw SampleDeviceEOFException(); // dead reader device?
  } else if (errno == ENOMEM) {
    throw "out of memory on select()"; /* need to handle this better */
  }
}

size_t
SampleStructFileSource::numBytesReady() const
{
  int size;

  if ( ioctl(fd, FIONREAD, &size) < 0 ) {
    throw SampleDeviceException();
  }

  return size;
}

int
SampleStructFileSource::numSamplesReady() const 
{
  return numBytesReady() / sizeof(SampleStruct);
}

const SampleStruct *
SampleStructFileSource::read(int num_secs_wait = -1) 
{
  int ret;
  size_t count;

  count = numBytesReady();

  /* the select() in waitForNewData() seems to hang forever? */
  if (count == 0) {
    waitForNewData(num_secs_wait);
    /* the select() inside waitForNewData timed out, so cop out early */
    if (! (count = numBytesReady())) return  read_memory;

  }
  

  if (count > max_read_memory) {
    if (read_memory) delete read_memory;
    max_read_memory = count;
    read_memory = (SampleStruct *)new char[max_read_memory];
  }


  
  if ( (ret = ::read(fd, read_memory, num_bytes_last_read = count)) < 0) {
    int saved_errno = errno;
    switch (saved_errno) {    
      // maybe some custom errors here?
      default:
	throw SampleDeviceException();
    }
  } else if (count != 0 && ret == 0) {
    throw SampleDeviceEOFException();
  }

  return read_memory;
    
}


void
SampleStructFileSource::init(const string & filename)
{
  open(filename);
}

void 
SampleStructFileSource::destruct()
{
  close();
}

void
SampleStructFileSource::open(const string & filename)
{
  fd = ::open (filename.c_str(), O_RDONLY);
  if ( fd  < 0 ) {
    throw SampleDeviceException();
  }
}

void 
SampleStructFileSource::close()
{
  ::close(fd);
}

SampleStructRTFSource::SampleStructRTFSource (unsigned short minor = 0) 
{
  strstream s;

  s << "/dev/rtf" << minor;
  
  string filename(s.str());
  init (filename);
}

void
SampleStructRTFSource::flush() {
  
  read(0);  
  
}
