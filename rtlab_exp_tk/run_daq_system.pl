#!/usr/bin/perl

die "Please set this script to be setuid root!\n" if ($> != 0);

$ENV{BASH_ENV} = $ENV{PATH} = "";

sub grab_pwd {
	my @pathstuff = split '/', $0;
	pop @pathstuff;
	my $ret = (join "/", @pathstuff);   
	$ret=~/(.*)/; # get rid of that tainted check stuff
	$ret = $1;
	return $ret;
}

sub rtp_is_loaded {
	if ( -e "/proc/modules" ) {
		open FH, "</proc/modules";
		while ($line = readline *FH) {
			if ($line =~ /^rt_process/) {
				close FH;
				return 1;
			}
		}
		close FH;	
	}
	return undef;
}

if ( ($pid = fork()) == 0 ) {
	my $dir = &grab_pwd;
	if ( (-e "${dir}/rt_process.o") && (-e "${dir}/daq_system") ) {
		system("/sbin/rmmod rt_process") if &rtp_is_loaded;
		system("/sbin/insmod ${dir}/rt_process.o");
		die "rt_process module could not load\n" unless &rtp_is_loaded;
		($<,$>) = ($>,$<); # swap real and effective uid
		$0 = "daq_system" && exec("${dir}/daq_system");
        } else {
	    my $msg;
	    $msg = "rt_process.o not found." if (! -e "${dir}/rt_process.o");
	    $msg ||= "daq_system binary not found.";
	    die   "$msg Compile it and rerun this script!\n";
	}
} else {
	waitpid($pid, 0);
	system("/sbin/rmmod rt_process") if rtp_is_loaded();
}

