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

#ifndef _EXCEPTION_H
#  define _EXCEPTION_H

#include <qstring.h>
#include "common.h"


/*
  Todo: This may be not-that-useful... 
  Re-evaluate the decision to go with this exceptions set of classes --
  if it turns out to make error-reporting useful to users, go with it,
  otherwise nix it...
*/

class Exception
{
 public:

  enum ErrorReportingMode { 
    _low_err_mode = 0,
    GUI,
    Console,
    _high_err_mode
  };

  Exception(const QString & briefMsg = "An Exception Ocurred",
	    const QString & fullMsg = "There was an internal error",
	    ErrorReportingMode m = GUI); 

  void showError() const;
 
  
 protected:
  QString _briefMsg, _fullMsg;
  ErrorReportingMode _errorReportingMode;

 public:
  const QString & briefMsg() const { return _briefMsg; }
  const QString & fullMsg() const { return _fullMsg; }
  ErrorReportingMode & errorReportingMode() { return _errorReportingMode; }
  ErrorReportingMode errorReportingMode() const { return _errorReportingMode; }


 private:
  void __showError() const;
};

class UnimplementedException : public Exception 
{
 public:
  UnimplementedException (const QString & brief = "Unimplemented Operation",
			  const QString & full = 
			  "There was an internal error due to an "
			  "unimplemented operation.",
			  ErrorReportingMode m = GUI); 

};


class NoComediDeviceException : public Exception
{
 public:
  NoComediDeviceException (const QString & brief = "Comedi Device Not Found",
			   const QString & full = 
			   "A required comedi device was missing or was "
			   "found in an inconsistent state.",
			   ErrorReportingMode m = GUI); 
};

/* generic exceptions with respect to rt_process */
class RTPException : public Exception
{
 public:
  RTPException (const QString & brief = "Error occurred with rt_process.o",
		const QString & full = "An operation upon/with the rt_process "
		"kernel module failed!",
		ErrorReportingMode m = GUI);
};

/* generic exceptions with respect to /dev/mbuff */
class MBuffException : public Exception
{
 public:
  MBuffException (const QString & mbuff_file, ErrorReportingMode m = GUI);
  
};


class SampleDeviceException : public Exception 
{
 public:
  SampleDeviceException(const QString & brief = "The sample device "
			"encountered an error.",
			const QString & full = "An error occurred reading "
			"samples from the sample input device.") 
    : Exception(brief, full) {};
};

class SampleDeviceEOFException : public SampleDeviceException 
{
 public:
  SampleDeviceEOFException(const QString & brief = "EOF reached on sample"
			   "device.",
			   const QString & full = "Reached end of file while "
			   "accessing the sample device.")
    : SampleDeviceException(brief, full) {};
};

class FileException : public Exception
{
 public:
  FileException ( const QString & brief = "Error working with a file.",
		  const QString & full = "A file operation failed." )
    : Exception(brief, full) {};
};

class SampleOutputFileException : public FileException
{
 public:
  SampleOutputFileException(const QString & brief = "There was an error "
			    "with the sample output file.",
			    const QString & full = "Some error occurred "
			    "when outputting samples to a file.") 
    : FileException(brief, full) {};
};

class FileNotFoundException : public FileException
{
 public:
  FileNotFoundException ( const QString & brief = "File not found.",
			  const QString & full = "A required or specified file"
			  " was not found." )
    : FileException(brief, full) {};
};

#endif
