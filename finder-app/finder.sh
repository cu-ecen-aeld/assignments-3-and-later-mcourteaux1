#!/bin/bash
if [ -z $1 ] || [ -z $2 ];
then
	echo "Usage: finder <directory> <search phrase>"
	exit 1
fi

if [ ! -d $1 ];
then
	echo "Please provide a valid directory as first parameter"
	exit 1
fi

num_files=$( grep -Rl $1 -e $2 | wc -l ) #find filename containing pattern -r(ecursive) -l (filename only)
num_lines=$( grep -R $1 -e $2 | wc -l ) #find filename containing pattern -r(ecursive) -l (filename only)

echo "The number of files are $num_files and the number of matching lines are $num_lines"
exit 0