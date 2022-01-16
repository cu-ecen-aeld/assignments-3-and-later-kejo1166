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
#include <syslog.h>

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
      printf("error1");
      syslog(LOG_ERR, "Error: Invalid number of arguments passed <path to file> <text string>");
      return 1; // Return error
   }

   // Assuming arg1 is valid, open file with write permissions.
   FILE *fd = fopen(argv[1], "w");
   if (fd == NULL)
   {
      syslog(LOG_ERR, "Error: Failed to open \"%s\"", argv[1]);
      return 1;
   }

   // Write string to file
   syslog(LOG_DEBUG, "Writing \"%s\" to \"%s\"", argv[2], argv[1]);
   if (fputs(argv[2], fd) < 0)
   {
      syslog(LOG_ERR, "Error: Failed to write \"%s\" to \"%s\"", argv[2], argv[1]);
      fclose(fd); // Close file
      return 1;
   }

   // Successfully wrote message to file.
   fclose(fd); // Close file
   return 0;
}

// ============================================================================
// STATIC FUNCTIONS
// ============================================================================
