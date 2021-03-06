/**
 * @file aesdsocket.c
 * @author Kenneth A. Jones
 * @date 2022-02-05
 *
 * @brief
 *      Reference https://github.com/pasce/daemon-skeleton-linux-c for making a
 *      C program into a daemon.
 * 
 *      Reference timer_thread.c (https://github.com/cu-ecen-aeld/aesd-lectures/blob/master/lecture9/timer_thread.c)
 *      Reference poll() and Jake Micheal for tip http://www.unixguide.net/unix/programming/2.1.2.shtml
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
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <stdarg.h>
#include <arpa/inet.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/queue.h>
#include <time.h>
#include <sys/time.h>
#include <poll.h>

// ============================================================================
// PRIVATE MACROS AND DEFINES
// ============================================================================

#define DEBUG

#define APP_NAME "aesdsocket"

#define BUFFER_SIZE 1024

#define TIMER_INTERVAL_SEC 10

// Socket configuration
#define PORT "9000"
#define FAMILY AF_INET
#define SOCKET_TYPE SOCK_STREAM
#define FLAGS AI_PASSIVE
#define MAX_CONNECTIONS 50

// Socket data storage
#define USE_AESD_CHAR_DEVICE 1
#ifdef USE_AESD_CHAR_DEVICE
    #define STORAGE_DATA_PATH "/dev/aesdchar"
#else
    #define STORAGE_DATA_PATH "/var/tmp/aesdsocketdata"
#endif

// ============================================================================
// PRIVATE TYPEDEFS
// ============================================================================

// SOCKET THREAD STATES
typedef enum
{
    SCKT_THREAD_IDLE = 0,
    SCKT_THREAD_RUNNING,
    SCKT_THREAD_DONE
} SOCKET_THREAD_STATES_T;
typedef struct
{
    int threadId;
    int threadResult;
    SOCKET_THREAD_STATES_T threadStatus;
    int clientfd;
    int fd;
    struct sockaddr_in clientAddr;
} THREAD_PARAMS_T;

typedef struct slist_data_s SLINK_DATA_T;
struct slist_data_s
{
    pthread_t thread;
    THREAD_PARAMS_T params;
    SLIST_ENTRY(slist_data_s)
    entries;
};

// ============================================================================
// STATIC VARIABLES
// ============================================================================

static int serverfd = -1;
static int clientfd = -1;
static struct addrinfo *pServerInfo;
static bool appShutdown = false;
static pthread_mutex_t writeMutex = PTHREAD_MUTEX_INITIALIZER; // Initialize mutex'

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================

// ============================================================================
// STATIC FUNCTION PROTOTYPES
// ============================================================================

/**
 * @brief Print message to syslog and terminal depending on configuration
 *
 * @param logType - Syslog error type
 * @param __fmt - Formatted string
 * @param ...
 */
static void log_message(int logType, const char *__fmt, ...);

/**
 * @brief Cleanup
 *
 */
static void cleanup(void);

/**
 * @brief Signal interrupt handler
 *
 * @param signo - Enum of signal caught
 */
static void sig_handler(int signo);

/**
 * @brief Handles all socket communication
 * 
 * @param pThreadParams - Pointer to thread parameters 
 */
static void *handle_socket_comms(void *pThreadParams);

/**
 * @brief Timer handler called at expiration of timer interval          
 * 
 * @param sigval 
 */
static void *handle_timer(void *args);

/**
 * @brief Acquire mutex
 * 
 * @return true 
 * @return false 
 */
static bool write_lock(void);

/**
 * @brief Release mutex
 * 
 * @return true 
 * @return false 
 */
static bool write_unlock(void);

// ============================================================================
// GLOBAL FUNCTIONS
// ============================================================================

int main(int argc, char **argv)
{
    struct addrinfo hints;
    int status = 0;
    bool runAsDaemon = false;
    int threadNdx = 1;
    int filefd = -1;
    //timer_t timerId = 0;
    SLINK_DATA_T *pNode;
    struct pollfd socketsToPoll[1];
    int socketPollSleepMs = 100;

    if ((argc >= 2) && (strcmp("-d", argv[1]) == 0))
        runAsDaemon = true;

    // Create logger
    openlog(APP_NAME, 0, LOG_USER);

    log_message(LOG_DEBUG, "Starting aesdsocket ...\n");

    // Configure signal interrupts
    sig_t result = signal(SIGINT, sig_handler);
    if (result == SIG_ERR)
    {
        log_message(LOG_ERR, "Error: could not register SIGINT errno=%d\n", result);
        cleanup();
        return -1;
    }

    result = signal(SIGTERM, sig_handler);
    if (result == SIG_ERR)
    {
        log_message(LOG_ERR, "Error: could not register SIGTERM errno=%d\n", result);
        cleanup();
        return -1;
    }

    // Clear data structure
    memset(&hints, 0, sizeof(hints));

    // Get server info for IP address
    hints.ai_family = FAMILY;
    hints.ai_socktype = SOCKET_TYPE;
    hints.ai_flags = FLAGS;
    status = getaddrinfo(NULL, PORT, &hints, &pServerInfo);
    if (status != 0)
    {
        log_message(LOG_ERR, "Error: getaddrinfo() %s\n", gai_strerror(status));
        cleanup();
        return -1;
    }

    // Open socket connection
    serverfd = socket(FAMILY, SOCKET_TYPE, 0);
    if (serverfd < 0)
    {
        log_message(LOG_ERR, "Error: opening socket, errno=%d\n", errno);
        freeaddrinfo(pServerInfo); // Free allocated address info
        cleanup();
        return -1;
    }

    // Set socket options to allow re use of address
    if (setsockopt(serverfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0)
    {
        log_message(LOG_ERR, "Error: could not set socket options, errno=%d\n", errno);
        freeaddrinfo(pServerInfo); // Free allocated address info
        cleanup();
        return -1;
    }

    // Bind device address to socket
    if (bind(serverfd, pServerInfo->ai_addr, pServerInfo->ai_addrlen) < 0)
    {
        log_message(LOG_ERR, "Error: binding socket reason=%s\n", strerror(errno));
        freeaddrinfo(pServerInfo); // Free allocated address info
        cleanup();
        return -1;
    }
    freeaddrinfo(pServerInfo); // Free allocated address info

    // Run program as daemon
    if (runAsDaemon)
    {
        log_message(LOG_DEBUG, "Running as daemon ...\n");
        daemon(0, 0);
    }

    // Listen for connection
    if (listen(serverfd, MAX_CONNECTIONS) < 0)
    {
        log_message(LOG_ERR, "Error: listening for connection errno=%d\n", errno);
        cleanup();
        return -1;
    }

    //create or open file to store received packets
    filefd = open(STORAGE_DATA_PATH, O_CREAT | O_RDWR | O_APPEND | O_TRUNC, 0766);
    if (filefd == -1)
    {
        log_message(LOG_ERR, "Error: could not create file '%s'\n", STORAGE_DATA_PATH);
        cleanup();
        return -1;
    }
    close(filefd); // Close file

    // Create timer thread
    //setup_timer(&filefd, &timerId);
    pthread_t timerThread;
    if (pthread_create(&timerThread, NULL, handle_timer, &filefd) != 0)
    {
        log_message(LOG_DEBUG, "Thread create timer thread\n");
        cleanup();
        return -1;
    }

    log_message(LOG_INFO, "Listening for clients on port %s ...\n", PORT);

    // Initialize link list
    SLIST_HEAD(slisthead, slist_data_s)
    scktHead;
    SLIST_INIT(&scktHead);

    // Setup up a poll of socket for events. This will allow signal terminations
    //

    socketsToPoll[0].fd = serverfd;
    socketsToPoll[0].events = POLLIN;

    // Listen for connection forever
    while (!appShutdown)
    {
        struct sockaddr_in clientAddr;                 // Specified address of accepted client
        socklen_t clientAddrSize = sizeof(clientAddr); // Size of client address

        // Poll socket for new events
        poll(socketsToPoll, 1, socketPollSleepMs);
        if (socketsToPoll[0].revents != POLLIN)
            continue;

        // Accept incoming connections.  accept() will block until connection
        clientfd = accept(serverfd, (struct sockaddr *)&clientAddr, &clientAddrSize);

        // Exit
        if (appShutdown)
            break;

        if (clientfd < 0)
        {
            log_message(LOG_ERR, "Error: failed to accept client errno=%d\n", errno);
            cleanup();
            return -1;
        }

        log_message(LOG_INFO, "Accepted connection from %s\n", inet_ntoa(clientAddr.sin_addr));

        // Setup parameters to pass to socket communication handler
        pNode = (SLINK_DATA_T *)malloc(sizeof(SLINK_DATA_T));
        if (pNode == NULL)
        {
            log_message(LOG_ERR, "Error: Could NOT allocate memory\n");
            continue; // Not necessary to exit program for this error
        }
        pNode->params.threadId = threadNdx;
        pNode->params.threadResult = 0;
        pNode->params.threadStatus = SCKT_THREAD_IDLE;
        pNode->params.clientfd = clientfd;
        pNode->params.fd = filefd;
        pNode->params.clientAddr = clientAddr;

        // Insert into link list
        SLIST_INSERT_HEAD(&scktHead, pNode, entries);

        // Create thread
        if (pthread_create(&(pNode->thread), NULL, handle_socket_comms, (void *)&(pNode->params)) != 0)
        {
            log_message(LOG_DEBUG, "Thread %d not created %d\n0", pNode->thread);
            close(pNode->params.clientfd);
            free(pNode);
            continue;
        }

        threadNdx++;
        if (threadNdx >= __INT32_MAX__)
            threadNdx = 1;

        // Search for complete threads and join them
        SLIST_FOREACH(pNode, &scktHead, entries)
        {
            if (pNode->params.threadStatus == SCKT_THREAD_DONE)
            {
                log_message(LOG_DEBUG, "Thread %d has completed with status %d\n",
                            pNode->params.threadId, pNode->params.threadResult);
                pthread_join(pNode->thread, NULL); // Thread has complete, join
                close(pNode->params.clientfd);
                log_message(LOG_INFO, "Thread %d -- Closed connection with %s\n", pNode->params.threadId,
                            inet_ntoa(pNode->params.clientAddr.sin_addr));
            }
        }
    }

    // Stop timer thread
    pthread_join(timerThread, NULL);

    // Cancel any running threads
    while (!SLIST_EMPTY(&scktHead))
    {
        pNode = SLIST_FIRST(&scktHead);
        SLIST_REMOVE_HEAD(&scktHead, entries);
        log_message(LOG_DEBUG, "Freeing node @ %p ...\n", pNode);
        free(pNode); // Free allocate memory
        pNode = NULL;
    }

    // Remove storage file
    // close(filefd);
#ifndef USE_AESD_CHAR_DEVICE
    log_message(LOG_INFO, "Removing \"%s\"\n", STORAGE_DATA_PATH);
    unlink(STORAGE_DATA_PATH);
#endif

    cleanup();
    return 0;
}

// ============================================================================
// STATIC FUNCTIONS
// ============================================================================
void log_message(int logType, const char *fmt, ...)
{
    char buf[256];

    va_list vl;
    va_start(vl, fmt);
    vsnprintf(buf, sizeof(buf), fmt, vl);
    va_end(vl);

    syslog(logType, "%s", buf);
#ifdef DEBUG
    printf("%s", buf);
#endif
}

void cleanup(void)
{
    // Close server socket
    if (serverfd > 0)
        close(serverfd);

    // Close client socket
    if (clientfd > 0)
        close(clientfd);

    // Remove mutex
    pthread_mutex_destroy(&writeMutex);

    log_message(LOG_INFO, "Terminated\n");

    // Close sys log
    closelog();
}

void sig_handler(int signo)
{
    log_message(LOG_INFO, "Caught signal %d, exiting ...\n", signo);
    appShutdown = true;
}

void *handle_socket_comms(void *pThreadParams)
{
    char buf[1024];
    ssize_t nRead;
    ssize_t nWrite;
    ssize_t spaceRemaining = sizeof(buf);
    char *pBuf;
    int streamPos = 0;
    THREAD_PARAMS_T *pTP = (THREAD_PARAMS_T *)pThreadParams;
    int fd = -1;

    pTP->threadStatus = SCKT_THREAD_RUNNING;
    pBuf = (char *)malloc(sizeof(char) * BUFFER_SIZE);
    if (pBuf == NULL)
    {
        log_message(LOG_ERR, "Thread %d -- Error: Could not allocate memory\n", pTP->threadId);
        pTP->threadResult = -1;
    }

    // Read data from socket
    while (pTP->threadResult == 0)
    {
        nRead = read(clientfd, buf, sizeof(buf));
        if (nRead < 0)
        {
            log_message(LOG_ERR, "Thread %d -- Error: reading from socket errno=%d\n", pTP->threadId, errno);
            goto on_error;
        }
        else if (nRead == 0)
            continue; // Nothing read

        log_message(LOG_DEBUG, "Thread %d -- socket rd: %d bytes\n", pTP->threadId, nRead);

        if (nRead > spaceRemaining)
        {
            // Increase memory size
            pBuf = (char *)realloc(pBuf, sizeof(char) * (streamPos + BUFFER_SIZE));
            if (pBuf == NULL)
            {
                log_message(LOG_ERR, "Thread %d -- Error: Could not reallocate memory\n", pTP->threadId);
                goto on_error;
            }
            spaceRemaining += spaceRemaining;
        }
        memcpy(&pBuf[streamPos], buf, nRead);
        streamPos += nRead;
        spaceRemaining -= nRead;

        if (strchr(buf, '\n'))
        { // Found new line character, no send file back
            break;
        }
    }

    //log_message(LOG_DEBUG, "String: %s", pBuf);

    // Write data to file
    if (!write_lock())
        goto on_error;

    fd = open(STORAGE_DATA_PATH, O_RDWR | O_CREAT | O_APPEND, 0644);
    if (fd == -1)
    {
        log_message(LOG_ERR, "Thread %d -- could not open file '%s'\n", STORAGE_DATA_PATH);
        goto on_error;
    }

    // Save data received from client
    nWrite = write(fd, pBuf, streamPos);
    if (nWrite == -1)
    {
        log_message(LOG_ERR, "Thread %d -- Error: writing to file\n", pTP->threadId);
        goto on_error;
    }

    lseek(fd, 0, SEEK_SET); // go to begining of file

    // Write data to socket
    int rdPos = 0;
    while (1)
    {

        nRead = read(fd, buf, BUFFER_SIZE); // read_file(buf, rdPos, sizeof(buf));

        if (nRead == 0)
        {
            // EOF reached, done
            break;
        }
        else if (nRead < 0)
        {
            log_message(LOG_ERR, "Thread %d -- Error: reading from \"%s\" errno=%d\n",
                        pTP->threadId, STORAGE_DATA_PATH, nRead);
            continue; // Continue reading data
        }
        else
        {
            // Write byte to socket
            nWrite = write(clientfd, buf, nRead);

            if (nWrite < 0)
            {
                log_message(LOG_ERR, "Thread %d -- Error: writing to client socket errno=%d\n", pTP->threadId, nWrite);
                continue; // Continue reading data
            }

            log_message(LOG_DEBUG, "Thread %d -- socket wr: %d bytes\n", pTP->threadId, nWrite);

            // Increment position into file
            rdPos += nWrite;
        }
    }

    close(fd);      // Close file
    write_unlock(); // Release lock
    // Close connection with client
    free(pBuf); // Done with allocated memory
    pTP->threadStatus = SCKT_THREAD_DONE;
    pthread_exit(NULL);

on_error:
    if (fd != -1)
        close(fd);      // Close file
    write_unlock(); // Release lock
    free(pBuf); // Done with allocated memory
    pTP->threadResult = -1;
    pTP->threadStatus = SCKT_THREAD_DONE;
    pthread_exit(NULL);
}

void *handle_timer(void *args)
{
    struct timespec currTime = {0, 0};
    int count = TIMER_INTERVAL_SEC;

    while (!appShutdown)
    {
        // Get current time
        if (clock_gettime(CLOCK_MONOTONIC, &currTime), 0)
        {
            log_message(LOG_ERR, "Error: could get monotonic time, [%s]\n", strerror(errno));
            continue;
        }

        if ((--count) <= 0)
        {
            count = TIMER_INTERVAL_SEC;
        }

        currTime.tv_sec += 1; 
        currTime.tv_nsec += 1000000;
        if (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &currTime, NULL) != 0)
        {
            // The sleep was interrupted by a signal handler; see signal(7).
            if (errno == EINTR)
                break; // Exit thread

            log_message(LOG_ERR, "Error: could not sleep, [%s]\n", strerror(errno));
        }        
    }

    log_message(LOG_INFO, "<<< Timer thread done  >>>\n");
    pthread_exit(NULL);
}

bool write_lock(void)
{
    if (pthread_mutex_lock(&writeMutex) != 0)
    {
        log_message(LOG_ERR, "Error: Could not acquire lock\n");
        return false;
    }
    return true;
}

bool write_unlock(void)
{
    if (pthread_mutex_unlock(&writeMutex) != 0)
    {
        log_message(LOG_ERR, "Error: Could not release lock\n");
        return false;
    }
    return true;
}
