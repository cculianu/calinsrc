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

#ifndef __PLUGIN_H
#define __PLUGIN_H

class QObject;

/* The plugin interface is minimalist. */
class Plugin
{
public:
  virtual ~Plugin() {};
  virtual const char *name() const = 0; /* returns the plugin's name */
  virtual const char *description() const = 0; /* returns the description */
};

/* all entry functions throw an Exception if they cannot be started */
typedef Plugin * (*plugin_entry_fn_t)(QObject *);

#define DS_PLUGIN_VER 0x01

/*
  Plugin rules:

  a plugin .so must contain the following symbols:

  o "ds_plugin_ver" - int indicating the version of the plugin engine
                      this plugin was written for.  Must equal 
                      DS_PLUGIN_VER preprocessor symbol defined in this file
  o "entry"         - the entry function of type plugin_enrty_fn_t, should
                      return a valid Plugin * reference (you need to
                      subcladd Plugin * to implement your plugin)
  o "name"          - the const char * friendly name of the plugin


  Any plugins missing the above symbols will fail to load!


  Optional symbols: 

  o "description"   - const char *, a brief description of the plugin's 
                      functionality
  o "author"        - const char *, authors of this plugin
  o "requires"      - const char *, a descriptive string explaining what is
                      required to properly load this module

*/
#endif
