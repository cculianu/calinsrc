/* todo: auto-generate this! */

#ifndef _CONFIG_H
#  define _CONFIG_H

#ifndef DAQ_DIRNAME
#define DAQ_DIRNAME "daq_system"
#endif

#ifndef DAQ_PREFIX
#define DAQ_PREFIX "/usr/local/"
#endif

#ifndef DAQ_DATA_PREFIX
#define DAQ_DATA_PREFIX "/tmp/"
#endif

#ifndef DAQ_ETC_PREFIX
#define DAQ_ETC_PREFIX DAQ_PREFIX "/etc/" DAQ_DIRNAME
#endif

#ifndef DAQ_RESOURCES_PREFIX
#define DAQ_RESOURCES_PREFIX DAQ_PREFIX "/lib/" DAQ_DIRNAME
#endif

#ifndef DAQ_PLUGINS_PREFIX
#define DAQ_PLUGINS_PREFIX DAQ_RESOURCES_PREFIX "/plugins/"
#endif

#endif
