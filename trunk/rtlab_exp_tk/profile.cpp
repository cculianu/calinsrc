/***************************************************************************
                          profile.cpp  -  description
                             -------------------
    begin                : Wed Feb 6 2002
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

#include <stdio.h>
#include "common.h"
#include "profile.h"


static map<string, ProfileStruct> profile_data;


void profile (const string & key, ProfileCmd cmd)
{
  static struct timeval tv; static struct timezone tz;
  static char avgt_buf[64], count_buf[64];

  // get the time at the earliest possible time..
  gettimeofday(&tv, &tz);

  map<string, ProfileStruct>::iterator it = profile_data.find(key);

  if (it == profile_data.end()) {
    profile_data[key] = ProfileStruct();
  }

  ProfileStruct & p = profile_data[key];

  switch (cmd) {
  case PROFILE_START:
      // get the time here again because we want to not have the overhead of the above code to interfere with the start time
      gettimeofday(&tv, &tz);

      p.last_time = tv.tv_sec * 1000000 + tv.tv_usec;
      p.count++;
    break;
  case PROFILE_END:
      p.avg_time += (uint64)(((tv.tv_sec * 1000000 + tv.tv_usec) - p.last_time) / (double)p.count);
    break;
  case PROFILE_PRINT_AVG:
      sprintf(avgt_buf, "%qu", p.avg_time); sprintf(count_buf, "%qu", p.count);
      cerr << "Avg time for operation '" << key << "' is " << avgt_buf << "usec after " << count_buf << " iterations." << endl;
    break;
  }
}
