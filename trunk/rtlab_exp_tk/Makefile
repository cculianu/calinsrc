#
# Makefile for DAQ System and the RTLab Experiment Toolkit
#
NON_RT_PROGRAM = daq_system

all:	Makefile.${NON_RT_PROGRAM} NON_RT_PROGRAM rt_process plugins ndstool

.buildvars:
	@./configure

Makefile.${NON_RT_PROGRAM}: 	${NON_RT_PROGRAM}.pro .buildvars
	@export `/bin/grep ^QTDIR= .buildvars` && \
	 export `/bin/grep ^QMAKESPEC= .buildvars` && \
		$$QTDIR/bin/qmake ${NON_RT_PROGRAM}.pro  -o Makefile.${NON_RT_PROGRAM}

NON_RT_PROGRAM: .buildvars
	@export `grep ^QTDIR= .buildvars` && \
		${MAKE} -f  Makefile.${NON_RT_PROGRAM}

rt_process:	.buildvars
	@${MAKE} -f  Makefile.rt_process

plugins:	.buildvars
	@export `grep ^QTDIR= .buildvars` && \
		${MAKE} -f Makefile.plugins 

clean:
	-rm -f *.so *.o moc_*.cpp ${NON_RT_PROGRAM} Makefile.${NON_RT_PROGRAM} 

config:
	@./configure

ndstool: Makefile.${NON_RT_PROGRAM} NON_RT_PROGRAM .buildvars
	make -f Makefile.ndstool 

superclean:
	
	make clean
