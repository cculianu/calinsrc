PROGRAM = graphtest

all:	tmake $PROGRAM  

tmake:	
	${TMAKE_LOCATION}tmake Makefile.tmake\
	-o Makefile.${PROGRAM}

$PROGRAM:	
	${MAKE} -f  Makefile.${PROGRAM}

clean:
	-rm -f *.o moc_${NON_RT_PROGRAM}.cpp ${PROGRAM}

