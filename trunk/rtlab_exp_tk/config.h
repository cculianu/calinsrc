/* todo: auto-generate this! */

#ifndef _CONFIG_H
#  define _CONFIG_H

#ifndef DAQ_DIRNAME
#define DAQ_DIRNAME "daq_system"
#endif

#ifndef RTLAB_PREFIX
#define RTLAB_PREFIX "/opt/rtlab/"
#endif

#ifndef RTLAB_DATA_PREFIX
#define RTLAB_DATA_PREFIX "/tmp/"
#endif

#ifndef DAQ_DATA_PREFIX
#define DAQ_DATA_PREFIX RTLAB_DATA_PREFIX
#endif

#ifndef RTLAB_ETC_PREFIX
#define RTLAB_ETC_PREFIX RTLAB_PREFIX "/etc/" 
#endif

#ifndef RTLAB_RESOURCES_PREFIX
#define RTLAB_RESOURCES_PREFIX RTLAB_PREFIX 
#endif

#ifndef DAQ_LOG_TEMPLATES_PREFIX
#define DAQ_LOG_TEMPLATES_PREFIX RTLAB_RESOURCES_PREFIX
#endif

#ifndef RTLAB_PLUGINS_PREFIX
#define RTLAB_PLUGINS_PREFIX RTLAB_RESOURCES_PREFIX "/plugins/"
#endif

#ifndef RTLAB_MODULES_PREFIX
#define RTLAB_MODULES_PREFIX RTLAB_RESOURCES_PREFIX "/modules/"
#endif

#endif
