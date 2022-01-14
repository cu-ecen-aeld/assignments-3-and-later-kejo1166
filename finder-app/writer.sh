#!/bin/bash
# Assignment 1
# Arthor: Kenneth Jones
# 1/13/22

# Summary
# - Accepts the following arguments: the first argument is a full path to a file (including filename) on the filesystem, 
# referred to below as writefile; the second argument is a text string which will be written within this file, referred 
# to below as writestr
# - Exits with value 1 error and print statements if any of the arguments above were not specified
# - Creates a new file with name and path writefile with content writestr, overwriting any existing file and creating 
# the path if it doesn’t exist. Exits with value 1 and error print statement if the file could not be created.

# Check the number of arguments
if [ "$#" -eq 0 ]; then
   echo "Error: No arguments provided. Usage finder.sh <PATH TO FILE> <TEXT STRING>"
   exit 1
fi

# Check for less than 2 arguments
if [ "$#" -lt 2 ]; then
   echo "Error: Not enough arguments provided.  2 arguments required"
   exit 1
fi

# Validate the argument is not empty
if [ ! -n "$1" ]; then
   echo "Error: Argument 1 is not a valid string"
   exit 1
fi

# Validate search string is not empty
if [ ! -n "$2" ]; then
   echo "Error: Argument 2 is not a valid string"
   exit 1
fi

# Assign argumemnts to variable
writefile=$1
writestr=$2

# Create file if it doesn't exits
if [ ! -e "$writefile" ]; then
   touch $writefile  
fi

# Write string to file
echo "Writing $writestr to $writefile ..."
echo $writestr > $writefile
exit 0

