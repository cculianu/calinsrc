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

sub mod_is_loaded($) {
	my $module = shift;
	if ( -e "/proc/modules" ) {
		open FH, "</proc/modules";
		while ($line = readline *FH) {
			if ($line =~ /^$module/) {
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
	if ( (-e "${dir}/rtlab.o") && (-e "${dir}/daq_system") && (-e "${dir}/avn_stim.o" ) ) {
		system("/sbin/rmmod avn_stim") if mod_is_loaded("avn_stim");
		system("/sbin/rmmod rtlab") if mod_is_loaded("rtlab");
		system("/sbin/insmod ${dir}/rtlab.o");
		system("/sbin/insmod ${dir}/avn_stim.o");
		die "rtlab module could not load\n" unless (mod_is_loaded("rtlab") && mod_is_loaded("avn_stim"));
		($<,$>) = ($>,$<); # swap real and effective uid
		$0 = "daq_system" && exec("${dir}/daq_system");
        } else {
	    my $msg;
	    $msg = "rtlab.o not found." if (! -e "${dir}/rtlab.o");
	    $msg = "avn_stim.o not found." if (! -e "${dir}/avn_stim.o");
	    $msg ||= "daq_system binary not found.";
	    die   "$msg Compile it and rerun this script!\n";
	}
} else {
	waitpid($pid, 0);
	system("/sbin/rmmod avn_stim")   if mod_is_loaded("avn_stim");
	system("/sbin/rmmod rtlab") if mod_is_loaded("rtlab");
}

