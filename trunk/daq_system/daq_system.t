#!
#!Some code to intelligently read rtl.mk
#!
#${
sub qt_dir_checks($) {
	my $base = shift;
 
	if ( ! defined $base || $base eq "" ) {
		goto err_ret;
	}
	
	my @dirs = ($base, "${base}/lib", "${base}/include");
	
	foreach my $dir (@dirs) {
		if ( ! defined $dir 
		     || ! -r $dir 
		     || ! -d $dir 
		     || ! -x $dir ) {
		err_ret:
			return undef;
		} 
	}
	
	return 1;
	
}
  if (! qt_dir_checks($ENV{'QTDIR'}) ) {
    die q|
------------------------------------------------------------------------------
ABORTING TMAKE
**************
The 'QTDIR' environment variable was not found (or was set to a bogus 
directory). 

Your QTDIR=| . ( $ENV{'QTDIR'} ? $ENV{'QTDIR'} : "<No Value Set>" ) . q|

Since this is a QT Project, this environment variable needs to be defined
and should point to your QT2 directory.  This is often (but not necessarily)
something like '/usr/lib/qt2', '/usr/local/lib/qt2' or '/usr/local/qt2'. It
also must contain the proper qt libraries in a subdirectory called 'lib' and
the proper qt development headers in a subdirectory called 'include'.
------------------------------------------------------------------------------
|;		
  }
  if (! -r '/usr/include/comedi.h' || ! -r '/usr/include/comedilib.h' ) {
    die q|
------------------------------------------------------------------------------
ABORTING TMAKE
**************
The required comedi header files were not found.  When comedi is properly 
installed, it copies two header files into /usr/include. They are 'comedi.h' 
and 'comedilib.h'.

These files need to be present for this build to succeed.  Please either
put these files in /usr/include or re-install comedi.  (Run 'make install' in 
the comedi and comedilib source directories).
------------------------------------------------------------------------------
|;
  }
  if (! -f $project{'RTL_MK'} || ! -r $project{'RTL_MK'} ) {
    __daq_system_no_rtl_mk_Death:
    die q|
------------------------------------------------------------------------------
ABORTING TMAKE
**************
There was a problem opening and/or reading your 'rtl.mk' file.

Please edit Makefile.tmake and modify the 'RTL_MK' project variable to
point to the 'rtl.mk' file for your rt-linux kernel.

'rtl.mk' was created when you built rt-linux, and was probably needed when you
installed comedi.

(You *DID* build rt-linux, didn't you?!)
------------------------------------------------------------------------------
|;
  }

  open FH, '<' . $project{'RTL_MK'}  || goto __daq_system_no_rtl_mk_Death;
  while (<FH>) {
    if (/\s*RTL_DIR\s*=\s*(\S*)\s*$/) {
      $rtl_dir = $1;
    } elsif (/\s*RTLINUX_DIR\s*=\s*(\S*)\s*$/) {
      $rtlinux_dir = $1;
    }
  } 
  close FH;
  if (! -e $rtl_dir) {
    die q|
------------------------------------------------------------------------------
ABORTING TMAKE
**************
'| . $rtl_dir . q|' was not found! 

Bogus defines in rtl.mk??
------------------------------------------------------------------------------
|;
  }
  AddIncludePath($rtl_dir . "/include");  
  Project("TMAKE_APP_FLAG = 1");
  open OFH, '>' . "./Makefile.rtl_dir" || die q|
------------------------------------------------------------------------------
ABORTING TMAKE
**************
Cannot open ./Makefile.rtl_dir for writing
------------------------------------------------------------------------------
|;
  print OFH "include " . $project{'RTL_MK'} . "\n";
  close OFH;
  undef $rtl_dir; undef $rtlinux_dir;

  if ( $project{'DAQ_HW'} ) {
	$project{'DEFINES'} .= " HW=" . $project{'DAQ_HW'};
  }
#$}

#!
#! This is a tmake template for building UNIX applications or libraries.
#!
#${
    Project('TMAKE_LIBS += $$LIBS');
    if ( Project("TMAKE_LIB_FLAG") && !Config("staticlib") ) {
	Project('CONFIG *= dll');
    } elsif ( Project("TMAKE_APP_FLAG") || Config("dll") ) {
	Project('CONFIG -= staticlib');
    }
    if ( Config("warn_off") ) {
	Project('TMAKE_CFLAGS += $$TMAKE_CFLAGS_WARN_OFF');
	Project('TMAKE_CXXFLAGS += $$TMAKE_CXXFLAGS_WARN_OFF');
    } elsif ( Config("warn_on") ) {
	Project('TMAKE_CFLAGS += $$TMAKE_CFLAGS_WARN_ON');
	Project('TMAKE_CXXFLAGS += $$TMAKE_CXXFLAGS_WARN_ON');
    }
    if ( Config("debug") ) {
	Project('TMAKE_CFLAGS += $$TMAKE_CFLAGS_DEBUG');
	Project('TMAKE_CXXFLAGS += $$TMAKE_CXXFLAGS_DEBUG');
	Project('TMAKE_LFLAGS += $$TMAKE_LFLAGS_DEBUG');
    } elsif ( Config("release") ) {
	Project('TMAKE_CFLAGS += $$TMAKE_CFLAGS_RELEASE');
	Project('TMAKE_CXXFLAGS += $$TMAKE_CXXFLAGS_RELEASE');
	Project('TMAKE_LFLAGS += $$TMAKE_LFLAGS_RELEASE');
    }
    if ( Project("TMAKE_INCDIR") ) {
	AddIncludePath(Project("TMAKE_INCDIR"));
    }
    if ( Config("qt") || Config("opengl") ) {
	Project('CONFIG *= x11lib');
	if ( Config("opengl") ) {
	    Project('CONFIG *= x11inc');
	}
    }
    if ( Config("x11") ) {
	Project('CONFIG *= x11lib');
	Project('CONFIG *= x11inc');
    }
    if ( Config("qt") ) {
	Project('CONFIG *= moc');
	AddIncludePath(Project("TMAKE_INCDIR_QT"));
	if ( !Config("debug") ) {
	    Project('DEFINES += NO_DEBUG');
	}
	if ( Config("opengl") ) {
	    Project("TMAKE_LIBDIR_QT") &&
		Project('TMAKE_LIBDIR_FLAGS *= -L$$TMAKE_LIBDIR_QT');
	    Project('TMAKE_LIBS *= $$TMAKE_LIBS_QT_OPENGL');
	}
	if ( !((Project("TARGET") eq "qt") && Project("TMAKE_LIB_FLAG")) ) {
	    Project("TMAKE_LIBDIR_QT") &&
		Project('TMAKE_LIBDIR_FLAGS *= -L$$TMAKE_LIBDIR_QT');
	    Project('TMAKE_LIBS *= $$TMAKE_LIBS_QT');
	}
    }
    if ( Config("opengl") ) {
	AddIncludePath(Project("TMAKE_INCDIR_OPENGL"));
	Project("TMAKE_LIBDIR_OPENGL") &&
	    Project('TMAKE_LIBDIR_FLAGS *= -L$$TMAKE_LIBDIR_OPENGL');
	Project('TMAKE_LIBS *= $$TMAKE_LIBS_OPENGL');
    }
    if ( Config("x11inc") ) {
	AddIncludePath(Project("TMAKE_INCDIR_X11"));
    }
    if ( Config("x11lib") ) {
	Project("TMAKE_LIBDIR_X11") &&
	    Project('TMAKE_LIBDIR_FLAGS *= -L$$TMAKE_LIBDIR_X11');
	Project('TMAKE_LIBS *= $$TMAKE_LIBS_X11');
    }
    if ( Config("moc") ) {
	$moc_aware = 1;
    }
    if ( !Project("TMAKE_RUN_CC") ) {
	Project('TMAKE_RUN_CC = $(CC) -c $(CFLAGS) $(INCPATH) -o $obj $src');
    }
    if ( !Project("TMAKE_RUN_CC_IMP") ) {
	Project('TMAKE_RUN_CC_IMP = $(CC) -c $(CFLAGS) $(INCPATH) -o $@ $<');
    }
    if ( !Project("TMAKE_RUN_CXX") ) {
	Project('TMAKE_RUN_CXX = $(CXX) -c $(CXXFLAGS) $(INCPATH) -o $obj $src');
    }
    if ( !Project("TMAKE_RUN_CXX_IMP") ) {
	Project('TMAKE_RUN_CXX_IMP = $(CXX) -c $(CXXFLAGS) $(INCPATH) -o $@ $<');
    }
    Project('TMAKE_FILETAGS = HEADERS SOURCES TARGET DESTDIR $$FILETAGS');
    StdInit();
    $project{"VERSION"} || ($project{"VERSION"} = "1.0.0");
    ($project{"VER_MAJ"},$project{"VER_MIN"},$project{"VER_PAT"})
	= $project{"VERSION"} =~ /(\d)\.(\d)\.(\d)/;
    ($project{"VER_MAJ"},$project{"VER_MIN"},$project{"VER_PAT"})
	= $project{"VERSION"} =~ /(\d)\.(\d)/ if !defined($project{"VER_PAT"});
    Project('DESTDIR_TARGET = $(TARGET)');
    if ( Project("TMAKE_APP_FLAG") ) {
	if ( Config("dll") ) {
	    Project('TARGET = $$TARGET.so');
	    Project("TMAKE_LFLAGS_SHAPP") ||
		($project{"TMAKE_LFLAGS_SHAPP"} = $project{"TMAKE_LFLAGS_SHLIB"});
	    Project("TMAKE_LFLAGS_SONAME") &&
		($project{"TMAKE_LFLAGS_SONAME"} .= $project{"TARGET"});
	}
	$project{"TARGET"} = $project{"DESTDIR"} . $project{"TARGET"};
    } elsif ( Config("staticlib") ) {
	$project{"TARGET"} = $project{"DESTDIR"} . "lib" .
			     $project{"TARGET"} . ".a";
	Project("TMAKE_AR_CMD") ||
	    Project('TMAKE_AR_CMD = $(AR) $(TARGET) $(OBJECTS) $(OBJMOC)');
    } else {
	$project{"TARGETA"} = $project{"DESTDIR"} . "lib" .
			      $project{"TARGET"} . ".a";
	if ( Project("TMAKE_AR_CMD") ) {
	    $project{"TMAKE_AR_CMD"} =~ s/\(TARGET\)/\(TARGETA\)/g;
	} else {
	    Project('TMAKE_AR_CMD = $(AR) $(TARGETA) $(OBJECTS) $(OBJMOC)');
	}
	if ( $project{"TMAKE_HPUX_SHLIB"} ) {
	    $project{"TARGET_x.y.z"} = "lib" . $project{"TARGET"} . ".sl";
	} else {
	    $project{"TARGET_"} = "lib" . $project{"TARGET"} . ".so";
	    $project{"TARGET_x"} = $project{"TARGET_"} . "." .
				   $project{"VER_MAJ"};
	    $project{"TARGET_x.y"} = $project{"TARGET_"} . "." .
	                             $project{"VER_MAJ"} . "." .
				     $project{"VER_MIN"};
	    $project{"TARGET_x.y.z"} = $project{"TARGET_"} . "." .
				     $project{"VERSION"};
	    $project{"TMAKE_LN_SHLIB"} = "-ln -s";
        }
	$project{"TARGET"} = $project{"TARGET_x.y.z"};
	if ( $project{"DESTDIR"} ) {
	    $project{"DESTDIR_TARGET"} = $project{"DESTDIR"} .
					 $project{"TARGET"};
	}
	Project("TMAKE_LFLAGS_SONAME") &&
	    ($project{"TMAKE_LFLAGS_SONAME"} .= $project{"TARGET_x"});
	$project{"TMAKE_LINK_SHLIB_CMD"} ||
	    ($project{"TMAKE_LINK_SHLIB_CMD"} =
	      '$(LINK) $(LFLAGS) -o $(TARGETD) $(OBJECTS) $(OBJMOC) $(LIBS)');
    }
    if ( Config("dll") ) {
	Project('TMAKE_CFLAGS *= $$TMAKE_CFLAGS_SHLIB' );
	Project('TMAKE_CXXFLAGS *= $$TMAKE_CXXFLAGS_SHLIB' );
	if ( Project("TMAKE_APP_FLAG") ) {
	    Project('TMAKE_LFLAGS *= $$TMAKE_LFLAGS_SHAPP');
	} else {
	    Project('TMAKE_LFLAGS *= $$TMAKE_LFLAGS_SHLIB $$TMAKE_LFLAGS_SONAME');
	}
    }
#$}
#!
# Makefile for building #$ Expand("TARGET")
# Generated by tmake at #$ Now();
#     Project: #$ Expand("PROJECT");
#    Template: #$ Expand("TEMPLATE");
#############################################################################

####### Compiler, tools and options

CC	=	#$ Expand("TMAKE_CC");
CXX	=	#$ Expand("TMAKE_CXX");
CFLAGS	=	#$ Expand("TMAKE_CFLAGS"); ExpandGlue("DEFINES","-D"," -D","");
CXXFLAGS=	#$ Expand("TMAKE_CXXFLAGS"); ExpandGlue("DEFINES","-D"," -D","");
INCPATH	=	#$ ExpandPath("INCPATH","-I"," -I","");
#$ Config("staticlib") && DisableOutput();
LINK	=	#$ Expand("TMAKE_LINK");
LFLAGS	=	#$ Expand("TMAKE_LFLAGS");
LIBS	=	#$ Expand("TMAKE_LIBDIR_FLAGS"); Expand("TMAKE_LIBS");
#$ Config("staticlib") && EnableOutput();
#$ Project("TMAKE_LIB_FLAG") || DisableOutput();
AR	=	#$ Expand("TMAKE_AR");
RANLIB	=	#$ Expand("TMAKE_RANLIB");
#$ Project("TMAKE_LIB_FLAG") || EnableOutput();
MOC	=	#$ Expand("TMAKE_MOC");

TAR	=	#$ Expand("TMAKE_TAR");
GZIP	=	#$ Expand("TMAKE_GZIP");

####### Files

HEADERS =	#$ ExpandList("HEADERS");
SOURCES =	#$ ExpandList("SOURCES");
OBJECTS =	#$ ExpandList("OBJECTS");
SRCMOC	=	#$ ExpandList("SRCMOC");
OBJMOC	=	#$ ExpandList("OBJMOC");
DIST	=	#$ ExpandList("DISTFILES");
TARGET	=	#$ Expand("TARGET");
#$ (Project("TMAKE_APP_FLAG") || Config("staticlib")) && DisableOutput();
TARGETA	=	#$ Expand("TARGETA");
TARGETD	=	#$ Expand("TARGET_x.y.z");
TARGET0	=	#$ Expand("TARGET_");
TARGET1	=	#$ Expand("TARGET_x");
TARGET2	=	#$ Expand("TARGET_x.y");
#$ (Project("TMAKE_APP_FLAG") || Config("staticlib")) && EnableOutput();

####### Implicit rules

.SUFFIXES: .cpp .cxx .cc .C .c

.cpp.o:
	#$ Expand("TMAKE_RUN_CXX_IMP");

.cxx.o:
	#$ Expand("TMAKE_RUN_CXX_IMP");

.cc.o:
	#$ Expand("TMAKE_RUN_CXX_IMP");

.C.o:
	#$ Expand("TMAKE_RUN_CXX_IMP");

.c.o:
	#$ Expand("TMAKE_RUN_CC_IMP");

####### Build rules

#$ Project("TMAKE_APP_FLAG") || DisableOutput();
all: #$ ExpandGlue("ALL_DEPS",""," "," "); $text .= '$(TARGET)';

$(TARGET): $(OBJECTS) $(OBJMOC) #$ Expand("TARGETDEPS");
	$(LINK) $(LFLAGS) -o $(TARGET) $(OBJECTS) $(OBJMOC) $(LIBS)
#$ Project("TMAKE_APP_FLAG") || EnableOutput();
#$ (Config("staticlib") || Project("TMAKE_APP_FLAG")) && DisableOutput();
all: #$ ExpandGlue("ALL_DEPS",""," ",""); Expand("DESTDIR_TARGET");

#$ Substitute('$$DESTDIR_TARGET: $(OBJECTS) $(OBJMOC) $$TARGETDEPS');
	-rm -f $(TARGET) $(TARGET0) $(TARGET1) $(TARGET2)
	#$ Expand("TMAKE_LINK_SHLIB_CMD");
	#$ ExpandGlue("TMAKE_LN_SHLIB",""," "," \$(TARGET) \$(TARGET0)");
	#$ ExpandGlue("TMAKE_LN_SHLIB",""," "," \$(TARGET) \$(TARGET1)");
	#$ ExpandGlue("TMAKE_LN_SHLIB",""," "," \$(TARGET) \$(TARGET2)");
	#${
	    $d = Project("DESTDIR");
	    if ( $d ) {
		$d =~ s-([^/])$-$1/-;
		if ( Project("TMAKE_HPUX_SHLIB") ) {
		    $text =  "-rm -f $d\$(TARGET)\n\t" .
			     "-mv \$(TARGET) $d";
		} else {
		    $text =  "-rm -f $d\$(TARGET)\n\t" .
			     "-rm -f $d\$(TARGET0)\n\t" .
			     "-rm -f $d\$(TARGET1)\n\t" .
			     "-rm -f $d\$(TARGET2)\n\t" .
			     "-mv \$(TARGET) \$(TARGET0) \$(TARGET1) \$(TARGET2) $d";
		}
	    }
	#$}

staticlib: $(TARGETA)

$(TARGETA): $(OBJECTS) $(OBJMOC) #$ Expand("TARGETDEPS");
	-rm -f $(TARGETA)
	#$ Expand("TMAKE_AR_CMD");
	#$ ExpandGlue("TMAKE_RANLIB","",""," \$(TARGETA)");
#$ (Config("staticlib") || Project("TMAKE_APP_FLAG")) && EnableOutput();
#$ Config("staticlib") || DisableOutput();
all: #$ ExpandGlue("ALL_DEPS",""," "," "); $text .= '$(TARGET)';

staticlib: $(TARGET)

$(TARGET): $(OBJECTS) $(OBJMOC) #$ Expand("TARGETDEPS");
	-rm -f $(TARGET)
	#$ Expand("TMAKE_AR_CMD");
	#$ ExpandGlue("TMAKE_RANLIB","",""," \$(TARGET)");
#$ Config("staticlib") || EnableOutput();

moc: $(SRCMOC)

#$ TmakeSelf();

dist:
	#$ Substitute('$(TAR) $$PROJECT.tar $$PROJECT.pro $(SOURCES) $(HEADERS) $(DIST)');
	#$ Substitute('$(GZIP) $$PROJECT.tar');

clean:
	-rm -f $(OBJECTS) $(OBJMOC) $(SRCMOC) $(TARGET)
#$ (Config("staticlib") || Project("TMAKE_APP_FLAG")) && DisableOutput();
	-rm -f $(TARGET0) $(TARGET1) $(TARGET2) $(TARGETA)
#$ (Config("staticlib") || Project("TMAKE_APP_FLAG")) && EnableOutput();
	#$ ExpandGlue("TMAKE_CLEAN","-rm -f "," ","");
	-rm -f *~ core
	#$ ExpandGlue("CLEAN_FILES","-rm -f "," ","");

####### Compile

#$ BuildObj(Project("OBJECTS"),Project("SOURCES"));
#$ BuildMocObj(Project("OBJMOC"),Project("SRCMOC"));
#$ BuildMocSrc(Project("HEADERS"));
#$ BuildMocSrc(Project("SOURCES"));
