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

#include <stdio.h>
#include "common.h"
#include <string>   
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <iostream.h>
#include <fcntl.h>
#include <errno.h>

/* this function is non-reentrant! */
string operator+(const string & s, uint64 in)
{ return s + string(uint64_to_cstr(in)); };

/* this function is non-reentrant! */
QString operator+(const QString & s, uint64 in)
{ return s + QString(uint64_to_cstr(in)); };


const char *uint64_to_cstr(uint64 in)
{
  static char buf[64]; // non-reentrant function!!

  snprintf(buf, 64, "%llu", in);
  buf[63] = 0;

  return buf;
}

bool cstr_to_uint64(const char *in, uint64 & out)
{
  if (sscanf(in, "%llu",  &out) != 1) return false;

  return true;
}

uint64 cstr_to_uint64(const char *in, bool * ok)
{
  bool k;
  uint64 tmp = 0;
  k = cstr_to_uint64(in, tmp);
  if (ok) *ok = k;
  return tmp;
}

/* Checks whether file can be opened for reading ('r') or writing ('w'). 
   Returns an empty sting if opened successfully, 
   an error message otherwise. */
string fileErrorMsg ( const char *file_name, char mode = 'r' )
{
  string message;
  string generic = "Error opening file.";
  if (mode == 'r') { //file opened for reading
    int file = open( file_name, O_RDONLY );
    
    if ( file != -1 )
      {
        close( file );
        cout <<"No error";
        return ""; 
      }
    if ( errno == EACCES ) //check file permissions
      {
        struct stat buf;
        if ( !stat( file_name, &buf ) )
          {
            if( ( buf.st_mode & S_IRUSR ) && !( buf.st_mode & S_IROTH ) 
                && !( buf.st_mode & S_IRGRP ) && getuid() ) 
              {
                //only user has read permission
                
                if ( getuid() != buf.st_uid )  //check if user id matches
                  
                  message = "Error opening file " + string( file_name ) + 
                    " for reading. Check user permissions.\n";
                else message = generic;
              }
            else if ( ( buf.st_mode & S_IRGRP ) && !( buf.st_mode & S_IROTH ) 
                      && getuid() ) 
              {
                //only group has read permission
                
                if ( getgid() != buf.st_gid ) //check if group id matches
                  message = "Error opening file " + string ( file_name ) + 
                    " for reading. Check group permissions.\n";
                else message = generic;
              }
            else if ( !( buf.st_mode & S_IRUSR ) && getuid() ) 
              //user does not have read permission
              {
                if ( getuid() == buf.st_uid )
                  message = "Error opening file " + string ( file_name ) + 
                    " for reading. Owner does not have read permission.\n";
              }
            else if ( !( buf.st_mode & S_IRGRP ) && getuid() ) 
              {
                //group does not have read permission
                if ( getgid() == buf.st_gid )
                  message = "Error opening file " + string ( file_name ) + 
                    " for reading. Group does not have read permission.\n";
              }
            else //unknown permissions problem              
              message = generic;
          }
        else
          {
            message = "Error opening file " + string ( file_name ) + 
              " for reading. " + string ( strerror( errno ) );
          }  
      }
    else 
      {
        message = "Error opening file " + string ( file_name ) + 
          " for reading. " + string ( strerror( errno ) );
      }
    
  } 
  
  else 
    { //file opened for writing
      if ( open(file_name, O_CREAT|O_WRONLY|O_EXCL, S_IWUSR) != -1 ) 
        {
          //file opened successfully 
          unlink( file_name );
          return ""; 
        }
      else 
        { 
          message = "Error writing to file " + string ( file_name ) + ". " + 
            string ( strerror( errno ) );
        }
    }
  return message;
}
