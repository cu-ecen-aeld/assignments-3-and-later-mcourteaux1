#!/bin/bash

# Check if the correct number of arguments are provided
if [ $# -ne 2 ]; then
    echo "Error: Missing parameters."
    echo "Usage: $0 <writefile> <writestr>"
    exit 1
fi

# Assign the arguments to variables
writefile=$1
writestr=$2

# Create the directory path for the file if it doesn't exist
dirpath=$(dirname "$writefile")
if [ ! -d "$dirpath" ]; then
    echo "Error: Directory path $dirpath does not exist. Creating it now."
    mkdir -p "$dirpath" || { echo "Error: Could not create directory $dirpath."; exit 1; }
fi

# Write the text string to the file (overwriting it if it exists)
echo "$writestr" > "$writefile" || { echo "Error: Could not write to file $writefile."; exit 1; }

# Success message
echo "Successfully written to $writefile."
