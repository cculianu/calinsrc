#!/bin/sh

vers=`cat VERSION | grep Revision: | cut -f 2 -d " " | cut -f 1 -d '$'`
if [ -z "$vers" ]; then
	cat VERSION
else
	echo $vers
fi
