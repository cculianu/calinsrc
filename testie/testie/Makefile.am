####### kdevelop will overwrite this part!!! (begin)##########
bin_PROGRAMS = testie
testie_SOURCES = testieview.cpp testiedoc.cpp testie.cpp main.cpp 
SOURCES = testieview.cpp testiedoc.cpp testie.cpp main.cpp 
INTERFACES = 
TRANSLATIONS = 
KDE_OPTIONS = qtonly
testie_LDADD   =  -lqt -lXext -lX11 $(LIBSOCKET)

SUBDIRS = docs 

EXTRA_DIST = main.cpp testie.cpp testie.h testiedoc.cpp testiedoc.h testieview.cpp testieview.h lo32-app-testie.png lo16-app-testie.png filenew.xpm filesave.xpm fileopen.xpm 

####### kdevelop will overwrite this part!!! (end)############


# set the include path for X, qt and KDE
INCLUDES= $(all_includes)

testie_METASOURCES = AUTO

# the library search path. 
testie_LDFLAGS = $(all_libraries) 

# them while "make clean", use CLEANFILES
DISTCLEANFILES = $(testie_METASOURCES)

KDE_OPTIONS = qtonly



