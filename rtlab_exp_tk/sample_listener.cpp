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
#include "common.h"
#include "shared_stuff.h"
#include "sample_listener.h"


SampleListener::~SampleListener()
{
  if (channel_ids) {
    delete channel_ids;    
    channel_ids = 0;
    size = 0;
  }
}

void
SampleListener::setChannelId(uint c)
{ 
  uint a[1];

  a[0] = c;  
  setChannelIds(a, 1);  
}

void 
SampleListener::setChannelIds(const uint chan_ids[], uint array_size)
{ 
  if (channel_ids) 
    delete channel_ids;  
  
  size = array_size;  
  channel_ids = new uint [size];
  
  for (uint i = 0; i < size; i++)
    channel_ids[i] = chan_ids[i];
}


void 
SampleListener::addChannelId(uint c) 
{
  size++;
  uint *new_a = new uint [size];
  new_a[size] = c;
  if (channel_ids) {
    for (uint i = 0; i < size - 1; i++)
      new_a[i] = channel_ids[i];
    delete channel_ids;
  }
  channel_ids = new_a;
}

void
SampleListener::removeChannelId(uint c)
{
  int idx = -1;
  
  for (uint i = 0; i < size; i++)
    if (channel_ids[i] == c) {
      idx = (int)i;
      break;
    }

  if (idx > -1) {
    uint *new_array = new uint [size-1];
    for (uint i = 0; i < size; i++) {
      if ((uint)idx < i)
	new_array[i-1] = channel_ids[i];
      else if ((uint)idx > i)
	new_array[i] = channel_ids[i];
    }
    size--;
    delete channel_ids;
    channel_ids = new_array;
  }
  
}
