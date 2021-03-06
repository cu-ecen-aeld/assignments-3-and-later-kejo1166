/**
 * @file systemcalls.c
 * @author Kenneth A. Jones
 * @date 2022-01-21
 * 
 * @brief Code for assignment 3 part 1.
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include <stdio.h>
#include <stdlib.h>

#include "systemcalls.h"
#include <syslog.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>

/**
 * @param cmd the command to execute with system()
 * @return true if the commands in ... with arguments @param arguments were executed 
 *   successfully using the system() call, false if an error occurred, 
 *   either in invocation of the system() command, or if a non-zero return 
 *   value was returned by the command issued in @param.
*/
bool do_system(const char *cmd)
{
   // Create syslog for logging
   openlog(NULL, 0, LOG_USER);

   int rtnVal = system(cmd);

   // Reference https://man7.org/linux/man-pages/man3/system.3.html for return values
   if ((NULL == cmd) && (0 == rtnVal))
   {
      syslog(LOG_ERR, "No shell is available");
      return false;
   }

   if (-1 == rtnVal)
   {
      syslog(LOG_ERR, "Child process could not be created");
      return false;
   }

   if (rtnVal > 0)
   {
      syslog(LOG_ERR, "Shell could not be executed in the child process");
      return false;
   }

   return true;
}

/**
* @param count -The numbers of variables passed to the function. The variables are command to execute.
*   followed by arguments to pass to the command
*   Since exec() does not perform path expansion, the command to execute needs
*   to be an absolute path.
* @param ... - A list of 1 or more arguments after the @param count argument.
*   The first is always the full path to the command to execute with execv()
*   The remaining arguments are a list of arguments to pass to the command in execv()
* @return true if the command @param ... with arguments @param arguments were executed successfully
*   using the execv() call, false if an error occurred, either in invocation of the 
*   fork, waitpid, or execv() command, or if a non-zero return value was returned
*   by the command issued in @param arguments with the specified arguments.
*/

bool do_exec(int count, ...)
{
   va_list args;
   va_start(args, count);
   char *command[count + 1];
   int i;
   for (i = 0; i < count; i++)
   {
      command[i] = va_arg(args, char *);
   }
   command[count] = NULL;

   va_end(args);

   // Create syslog for logging
   openlog(NULL, 0, LOG_USER);

   // Create child process
   pid_t pid;
   int status;
   int rtnVal;
   bool exitVal = true;

   // Create child process
   pid = fork();

   if (-1 == pid)
   {
      syslog(LOG_ERR, "No child process is created");
      exitVal = false;
   }
   else if (0 == pid)
   {
      // Return in the child process.
      rtnVal = execv(command[0], command); // Note execv functions return only if an error has occurred
      syslog(LOG_ERR, "Error running child processes");
      exit(EXIT_FAILURE); // exit child process
   }
   else
   {
      // Returned in the parent process, need to wait for child process to finish
      rtnVal = waitpid(pid, &status, 0);

      if (-1 == rtnVal)
      {
         syslog(LOG_ERR, "Child process failed");
         exitVal = false;
      }

      // Check if the child process exited normally
      if (!WIFEXITED(status))
      {
         syslog(LOG_ERR, "Child process exit it with issues, exit status=%d ", WEXITSTATUS(status));
         exitVal = false;
      }
      else
      {
         if (WEXITSTATUS(status))
         {
            // child process exit with nonzero return
            syslog(LOG_INFO, "child process WEXITSTATUS %d", WEXITSTATUS(status));
            exitVal = false;
         }
      }
   }

   return exitVal;
}

/**
* @param outputfile - The full path to the file to write with command output.  
*   This file will be closed at completion of the function call.
* All other parameters, see do_exec above
*/
bool do_exec_redirect(const char *outputfile, int count, ...)
{
   va_list args;
   va_start(args, count);
   char *command[count + 1];
   int i;
   for (i = 0; i < count; i++)
   {
      command[i] = va_arg(args, char *);
   }
   command[count] = NULL;

   va_end(args);

   /*
    *
    *   Call execv, but first using https://stackoverflow.com/a/13784315/1446624 as a refernce,
    *   redirect standard out to a file specified by outputfile.
    *   The rest of the behaviour is same as do_exec()
    *
    */

   // Create syslog for logging
   openlog(NULL, 0, LOG_USER);

   // Create child process
   pid_t pid;
   int status;
   int rtnVal;
   bool exitVal = true;
   int fd = open(outputfile, O_WRONLY | O_CREAT);

   if (fd < 0)
   {
      syslog(LOG_ERR, "Failed to open");
      exitVal = false;
   }

   // Create child process
   pid = fork();

   if (-1 == pid)
   {
      syslog(LOG_ERR, "No child process is created");
      exitVal = false;
   }
   else if (0 == pid)
   {
      // Return in the child process.
      if (dup2(fd, 1) < 0)
      {
         syslog(LOG_ERR, "Failed dup2");
      }
      close(fd);
      rtnVal = execv(command[0], command); // Note execv functions return only if an error has occurred
      syslog(LOG_ERR, "Error running child processes");
      exit(EXIT_FAILURE); // exit child process
   }
   else
   {
      // Returned in the parent process, need to wait for child process to finish
      rtnVal = waitpid(pid, &status, 0);

      if (-1 == rtnVal)
      {
         syslog(LOG_ERR, "Child process failed");
         exitVal = false;
      }

      // Check if the child process exited normally
      if (!WIFEXITED(status))
      {
         syslog(LOG_ERR, "Child process exit it with issues, exit status=%d ", WEXITSTATUS(status));
         exitVal = false;
      }
      else
      {
         if (WEXITSTATUS(status))
         {
            // child process exit with nonzero return
            syslog(LOG_INFO, "child process WEXITSTATUS %d", WEXITSTATUS(status));
            exitVal = false;
         }
      }
   }

   return exitVal;
}
