/*
 * This file is part of the RT-Linux Multichannel Data Acquisition System
 *
 * Copyright (C) 1999,2000 David Christini
 * Copyright (C) 2001 David Christini, Lyuba Golub, Calin Culianu
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
#ifndef _SAMPLE_LISTENER_H
# define _SAMPLE_LISTENER_H

#include "common.h"
#include "shared_stuff.h"

/* Just a basic 'interface' class with all pure virtual methods */
class SampleListener
{
 public:
  
  virtual ~SampleListener();

  enum ChannelIds {
    MultiChannelListener = UINT_MAX - 1U,
    TheNullChannelId
  };

  /* means a newsample is ready for this listener */
  virtual void newSample(const SampleStruct *s) = 0;

  uint channelId() const 
    { 
      return (isMultiChannelListener() 
	      ? MultiChannelListener
	      : (size == 0 ? TheNullChannelId : channel_ids[0]));
    };

  void setChannelId(uint c);

  bool isMultiChannelListener() const { return (size > 1); };

  const uint * channelIds(uint & size_of_return_array) const 
    { size_of_return_array = size; return channel_ids; };

  void setChannelIds(const uint chan_ids[], uint array_size);

  void addChannelId(uint c);
  
  void removeChannelId(uint c);
  
  /* todo: add some methods if we think we may need them */

 protected:
  SampleListener() : channel_ids(0), size(0) { };

 private:
  uint *channel_ids;
  uint size;
};

#endif
