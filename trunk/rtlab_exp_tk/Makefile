#
# Edit Makefile.tmake and Makefile.rt_process to suit your tastes
#
NON_RT_PROGRAM = daq_system

TMAKE_LOCATION = ./

all:	Makefile.${NON_RT_PROGRAM} $NON_RT_PROGRAM rt_process plugins

Makefile.${NON_RT_PROGRAM}: 	Makefile.tmake
	@${TMAKE_LOCATION}tmake Makefile.tmake -o Makefile.${NON_RT_PROGRAM}

$NON_RT_PROGRAM: 	
	@${MAKE} -f  Makefile.${NON_RT_PROGRAM}

rt_process: 
	@${MAKE} -f  Makefile.rt_process

plugins:  
	@${MAKE} -f Makefile.plugins 
clean:
	-rm -f *.so *.o moc_*.cpp ${NON_RT_PROGRAM} Makefile.${NON_RT_PROGRAM}

