/**
 * @file writer.c
 * @author Kenneth A. Jones
 * @date 2022-01-15
 *
 * @brief Simple application that writes a specified string to a specified file.
 *
 * @copyright Copyright (c) 2022
 *
 */

// ============================================================================
// INCLUDES
// ============================================================================
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <syslog.h>
#include <string.h>
#include <unistd.h>

// ============================================================================
// PRIVATE MACROS AND DEFINES
// ============================================================================

#define MAX_ARGC 3

// ============================================================================
// PRIVATE TYPEDEFS
// ============================================================================

// ============================================================================
// STATIC VARIABLES
// ============================================================================

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================

// ============================================================================
// STATIC FUNCTION PROTOTYPES
// ============================================================================

// ============================================================================
// GLOBAL FUNCTIONS
// ============================================================================

int main(int argc, char **argv)
{
   // Create logger
   openlog(NULL, 0, LOG_USER);

   // Validate enough arguments have been passed
   if (argc < MAX_ARGC)
   {
      syslog(LOG_ERR, "Error: Invalid number of arguments passed <path to file> <text string>");
      closelog(); // Close sys log
      return 1; // Return error
   }

   // Assuming arg1 is valid, open file with write permissions.
   int fd = creat(argv[1], 0755);
   if (fd < 0)
   {
      syslog(LOG_ERR, "Error: Creating file \"%s\"", argv[1]);
      closelog(); // Close sys log
      return 1;
   }

   fd = open(argv[1], O_WRONLY);
   if (fd < 0)
   {
      syslog(LOG_ERR, "Error: Failed to open \"%s\"", argv[1]);
      closelog(); // Close sys log
      return 1;
   }

   // Write string to file
   syslog(LOG_DEBUG, "Writing \"%s\" to \"%s\"", argv[2], argv[1]);
   if (write(fd, (char*)argv[2], strlen(argv[2])) < 0)
   {
      syslog(LOG_ERR, "Error: Failed to write \"%s\" to \"%s\"", argv[2], argv[1]);
      close(fd); // Close file
      closelog(); // Close sys log
      return 1;
   }

   // Successfully wrote message to file.
   close(fd); // Close file
   closelog(); // Close sys log
   return 0;
}

// ============================================================================
// STATIC FUNCTIONS
// ============================================================================
