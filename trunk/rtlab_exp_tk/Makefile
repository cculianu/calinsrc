#
# Edit Makefile.tmake and Makefile.rt_process to suit your tastes
#
NON_RT_PROGRAM = daq_system

TMAKE_LOCATION = ./

all:	Makefile.${NON_RT_PROGRAM} $NON_RT_PROGRAM rt_process.o 

Makefile.${NON_RT_PROGRAM}: 	Makefile.tmake
	${TMAKE_LOCATION}tmake Makefile.tmake -o Makefile.${NON_RT_PROGRAM}

$NON_RT_PROGRAM:	
	${MAKE} -f  Makefile.${NON_RT_PROGRAM}

rt_process.o: 
	${MAKE} -f  Makefile.rt_process

clean:
	-rm -f *.o moc_*.cpp ${NON_RT_PROGRAM} Makefile.${NON_RT_PROGRAM}

