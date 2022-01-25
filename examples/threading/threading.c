/**
 * @file threading.c
 * @author Kenneth A. Jones
 * @date 2022-01-25
 * 
 * @brief Implementation of threading for assignment 4.  Modified original code provide by ECEN 5713 
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// Macro to convert to microseconds
#define MSECTOUSEC(x) (x * 1000)

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{
   // Wait, obtain mutex, wait, release mutex as described by thread_data structure
   // hint: use a cast like the one below to obtain thread arguments from your parameter
   // struct thread_data* thread_func_args = (struct thread_data *) thread_param;
   int res;

   // Cast thread parameters to thread data structure
   struct thread_data *pThreadParams = (struct thread_data *) thread_param;

   // Wait predetermined time before acquiring lock
   res = usleep(pThreadParams->wait_before_lock_ms * 1000);
   if (0 != res)
   {
      ERROR_LOG("Failed to suspend thread for %dms", pThreadParams->wait_before_lock_ms);
      pThreadParams->thread_complete_success = false;
      return thread_param;
   }

   // Obtain thread mutex lock
   res = pthread_mutex_lock(pThreadParams->thread_mutex);
   if (0 != res)
   {
      ERROR_LOG("Failed to obtain thread mutex lock");
      pThreadParams->thread_complete_success = false;
      return thread_param;
   }

   // Wait predetermined time after releasing lock
   res = usleep(pThreadParams->wait_after_lock_ms * 1000);
   if (0 != res)
   {
      ERROR_LOG("Failed to suspend thread for %dms", pThreadParams->wait_after_lock_ms);
      pThreadParams->thread_complete_success = false;
      return thread_param;
   }

   // Release thread lock
   res = pthread_mutex_unlock(pThreadParams->thread_mutex);
   if (0 != res)
   {
      ERROR_LOG("Failed to release thread mutex lock");
      pThreadParams->thread_complete_success = false;
      return thread_param;
   }

   // Thread has successfully completed
   pThreadParams->thread_complete_success = true;
   return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
   /**
    * Allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
    * using threadfunc() as entry point.
    *
    * return true if successful.
    *
    * See implementation details in threading.h file comment block
    */

   int res;
   struct thread_data *pThreadDataParam;

   // Allocated memory for data structure
   pThreadDataParam = (struct thread_data *)malloc(sizeof(struct thread_data));
   if (NULL == pThreadDataParam)
   {
      ERROR_LOG("Failed to allocated memory for thread data structure");
      return false;
   }

   // Copy parameters
   pThreadDataParam->wait_before_lock_ms = wait_to_obtain_ms;
   pThreadDataParam->wait_after_lock_ms = wait_to_release_ms;
   pThreadDataParam->thread_mutex = mutex;

   // Create thread using threadfunc() and pThreadDataParam as arguments
   res = pthread_create(thread, NULL, &threadfunc, (void *)pThreadDataParam);
   if (0 != res)
   {
      ERROR_LOG("Failed to create new thread");
      return false;
   }

   return true;
}

