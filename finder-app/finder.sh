#!/bin/bash
if [ $# -lt 2 ]; 
    then exit 1
elif [ ! -d "$1" ]; 
    then exit 1
fi

filesdir=$1
searchstr=$2

echo "The number of files are $(find ${filesdir} -type f | wc -l) and the number of matching lines are $(grep -roh "${searchstr}" "${filesdir}" 2>/dev/null | wc -l)"
