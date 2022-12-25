#!/bin/bash

WFILE=$1
WSTR=$2

if [ -z $WFILE ] || [ -z $WSTR ];
then
	echo "The required argument(s) were not specified"
	exit 1
fi

WDIR=$(dirname $WFILE)

if [ ! -d $WDIR ];
then
	mkdir -p $WDIR
fi

touch $WFILE

if [ ! -f $WFILE ]
then 
	echo "The file does not exist"
	exit 1
fi

echo $WSTR >> $WFILE




