/***************************************************************************
                          profile.h  -  Profiling code
                             -------------------
    begin                : Tue Feb 5 2002
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

#ifndef _PROFILE_H
#define _PROFILE_H

#include <string>
#include <sys/time.h>
#include <unistd.h>

#include "common.h"

enum ProfileCmd {
  PROFILE_START,
  PROFILE_END,
  PROFILE_PRINT_AVG
};

struct ProfileStruct {
  ProfileStruct() : last_time(0), avg_time(0), count(0) {};
  uint64 last_time;
  uint64 avg_time;
  uint64 count;
};

void profile (const string & key, ProfileCmd cmd);
void profileStats();

#endif
