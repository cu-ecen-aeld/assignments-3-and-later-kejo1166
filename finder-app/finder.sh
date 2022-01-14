#!/bin/bash
# Assignment 1
# Arthor: Kenneth Jones
# 1/13/22

# Summary
# - Accepts the following runtime arguments: the first argument is a path to a directory on the filesystem, referred to 
#   below as filesdir; the second argument is a text string which will be searched within these files, referred to below 
#   as searchstr
# -Exits with return value 1 error and print statements if any of the parameters above were not specified
# -Exits with return value 1 error and print statements if filesdir does not represent a directory on the filesystem
# -Prints a message "The number of files are X and the number of matching lines are Y" where X is the number of files in 
#  the directory and all subdirectories and Y is the number of matching lines found in respective files.

# Check the number of arguments
if [ "$#" -eq 0 ]; then
   echo "Error: No arguments provided. Usage finder.sh <DIRECTORY PATH> <SEARCH STRING>"
   exit 1
fi

# Check for less than 2 arguments
if [ "$#" -lt 2 ]; then
   echo "Error: Not enough arguments provided.  2 arguments required"
   exit 1
fi

# Validate the direcroty argument is not empy and a directory
if [ ! -n "$1" ] || [ ! -d "$1" ]; then
   echo "Error: $1 is not a valid directory"
   exit 1
fi

# Validate search string is not empty
if [ -z "$2" ]; then
   echo "Error: Argument 2 cannot be empty"
   exit 1
fi

# Assign argumemnts to variable
filesdir=$1
searchstr=$2

# Get the number of files in the directory and number of matchines.   Print results
echo "The number of files are $(find "$filesdir" -type f | wc -l) and the number of matching lines are $(grep -or "$searchstr" "$filesdir" | wc -w)"
exit 0

