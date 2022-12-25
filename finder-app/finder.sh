#!/bin/bash

FDIR=$1      #filesdir
SSTR=$2	     #searchstr

if [ -z $FDIR ] || [ -z $SSTR ];
then
	echo "The required argument(s) were not specified"
	exit 1
fi

if ! [ -d $FDIR ];
then
	echo "The directory does not exist"
	exit 1
fi

FCOUNT=$( find $FDIR -type f | wc -l )
LCOUNT=$( grep -Rs $SSTR $FDIR | wc -l )

echo The number of files are $FCOUNT and the number of matching lines are $LCOUNT
