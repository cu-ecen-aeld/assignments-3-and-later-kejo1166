/**
 * @file aesdsocket.c
 * @author Kenneth A. Jones
 * @date 2022-02-05
 *
 * @brief
 *      Reference https://github.com/pasce/daemon-skeleton-linux-c for making a
 *      C program into a daemon.
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

// ============================================================================
// PRIVATE MACROS AND DEFINES
// ============================================================================

#define DEBUG

#define APP_NAME "aesdsocket"

// Socket configuration
#define PORT "9000"
#define FAMILY AF_INET
#define SOCKET_TYPE SOCK_STREAM
#define FLAGS AI_PASSIVE
#define MAX_CONNECTIONS 10

// Socket data storage
#define STORAGE_DATA_PATH "/var/tmp/aesdsocketdata"

// ============================================================================
// PRIVATE TYPEDEFS
// ============================================================================

typedef struct
{
    int clientfd;
    struct sockaddr_in clientAddr;
} THREAD_PARAMS_T;

// ============================================================================
// STATIC VARIABLES
// ============================================================================

static int serverfd = -1;
static int clientfd = -1;
static struct addrinfo *pServerInfo;
pthread_mutex_t writeMutex = PTHREAD_MUTEX_INITIALIZER; // Initialize mutex'

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
 * @brief Create a storage file
 *
 * @return int - 0 upon success; otherwise, error number
 *
 */
static int create_file(void);

/**
 * @brief Save data to storage file
 *
 * @param pBuf - Pointer stream of data
 * @param size - size of stream
 * @return int - 0 upon success; otherwise, error number
 */
static int write_file(char *pBuf, ssize_t size);

/**
 * @brief Read from storage file
 *
 * @param pBuf -Pointer to buffer
 * @param offset - Byte offset into file
 * @param maxSize - Maximum read size
 * @return int
 */
static int read_file(char *pBuf, int offset, ssize_t maxSize);

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
 * @param args 
 */
static void handle_socket_comms(void *args);

// ============================================================================
// GLOBAL FUNCTIONS
// ============================================================================

int main(int argc, char **argv)
{
    struct addrinfo hints;
    int status = 0;
    bool runAsDaemon = false;

    if ((argc >= 2) && (strcmp("-d", argv[1]) == 0))
        runAsDaemon = true;

    // Create logger
    openlog(APP_NAME, 0, LOG_USER);

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

    // Create storage file
    status = create_file();
    if (status != 0)
    {
        log_message(LOG_ERR, "Error: creating storeage file errno=%d\n", status);
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
        cleanup();
        return -1;
    }

    // Set socket options to allow re use of address
    if (setsockopt(serverfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0)
    {
        log_message(LOG_ERR, "Error: could not set socket options, errno=%d\n", errno);
        cleanup();
        return -1;
    }

    // Bind device address to socket
    if (bind(serverfd, pServerInfo->ai_addr, pServerInfo->ai_addrlen) < 0)
    {
        log_message(LOG_ERR, "Error: binding socket errno=%d\n", errno);
        cleanup();
        return -1;
    }

    // Listen for connection
    if (listen(serverfd, MAX_CONNECTIONS) < 0)
    {
        log_message(LOG_ERR, "Error: listening for connection errno=%d\n", errno);
        cleanup();
        return -1;
    }

    // Run program as daemon
    if (runAsDaemon)
        daemon(0, 0);

    // Listen for connection forever
    while (1)
    {
        struct sockaddr_in clientAddr;                 // Specified address of accepted client
        socklen_t clientAddrSize = sizeof(clientAddr); // Size of client address

        log_message(LOG_INFO, "Listening for clients on port %s ...", PORT);

        // Accept incoming connections.  accept() will block until connection
        clientfd = accept(serverfd, (struct sockaddr *)&clientAddr, &clientAddrSize);
        if (clientfd < 0)
        {
            log_message(LOG_ERR, "Error: failed to accept client errno=%d\n", errno);
            cleanup();
            return -1;
        }

        log_message(LOG_INFO, "Accepted connection from %s", inet_ntoa(clientAddr.sin_addr));

        // Setup parameters to pass to socket communication handler
        THREAD_PARAMS_T *args = (THREAD_PARAMS_T *)malloc(sizeof(THREAD_PARAMS_T));
        if (args == NULL)
        {
            log_message(LOG_ERR, "Error: Could NOT allocate memory");
            continue; // Not neccessary to exit program for this error
        }
        args->clientfd = clientfd;
        args->clientAddr = clientAddr;

        // Handle socket communication
        handle_socket_comms((void *)args);

        free(args); // Release allocated memory
    }

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
    printf("%s\n", buf);
#endif
}

int create_file(void)
{
    // File does not exist, so it must be created.
    int fd = creat(STORAGE_DATA_PATH, 0766);
    if (fd < 0)
        return errno;

    close(fd);
    return 0;
}

int write_file(char *pBuf, ssize_t size)
{
    if (pthread_mutex_lock(&writeMutex) != 0)
    {
        log_message(LOG_ERR, "Error: Could not acquire lock");
        return -1;
    }

    int fd = open(STORAGE_DATA_PATH, O_WRONLY);
    if (fd < 0)
    {
        return errno;
    }

    // Move to end of file
    lseek(fd, 0, SEEK_END);

    // Write data to file
    int nWrite = write(fd, pBuf, size);
    close(fd);

    if (pthread_mutex_unlock(&writeMutex) != 0)
    {
        log_message(LOG_ERR, "Error: Could not release lock");
        return -1;
    }

    return nWrite;
}

int read_file(char *pBuf, int offset, ssize_t maxSize)
{
    // Open file
    int fd = open(STORAGE_DATA_PATH, O_RDONLY);
    if (fd < 0)
        return -1;

    // Move to position
    lseek(fd, offset, SEEK_SET);

    int nRead = read(fd, pBuf, maxSize);
    close(fd);
    return nRead;
}

void cleanup(void)
{
    // Close server socket
    if (serverfd > 0)
        close(serverfd);

    // Close client socket
    if (clientfd > 0)
        close(clientfd);

    // Free allocated address info
    if (pServerInfo)
        freeaddrinfo(pServerInfo);

    // Remove storage file
    log_message(LOG_INFO, "Removing \"%s\"", STORAGE_DATA_PATH);
    unlink(STORAGE_DATA_PATH);

    log_message(LOG_INFO, "Terminated");

    // Close sys log
    closelog();
}

void sig_handler(int signo)
{
    log_message(LOG_INFO, "Caught signal %d, exiting ...", signo);
    cleanup();
    exit(0);
}

void handle_socket_comms(void *args)
{
    char buf[1024];
    ssize_t nRead;
    ssize_t nWrite;
    ssize_t spaceRemaining = sizeof(buf);
    int streamPos = 0;
    int status;
    THREAD_PARAMS_T *pTP = (THREAD_PARAMS_T *)args;

    // Read data from socket
    while (1)
    {
        nRead = read(clientfd, &buf[streamPos], spaceRemaining);
        if (nRead < 0)
        {
            log_message(LOG_ERR, "Error: reading from socket errno=%d\n", errno);
            streamPos = 0;
            spaceRemaining = sizeof(buf);
            continue; // Continue reading data
        }
        else if (nRead == 0)
            continue; // Nothing read

        log_message(LOG_DEBUG, "socket rd: %d bytes", nRead);

        // Save data received from client
        status = write_file(&buf[streamPos], nRead);
        if (status < 0)
        {
            // Error reading from file
            log_message(LOG_ERR, "Error: saving data to file errno=%d\n", status);
            continue; // Continue reading data
        }

        if (strchr(&buf[streamPos], '\n'))
        { // Found new line character, no send file back
            break;
        }
        else if (nRead == status)
        {
            // All data was saved, reset
            streamPos = 0;
            spaceRemaining = sizeof(buf);
        }
        else
        {
            // All data wasn't saved
            streamPos += nRead;
            spaceRemaining -= nRead;
        }
    }

    // Write data to socket
    int rdPos = 0;
    streamPos = 0;
    spaceRemaining = sizeof(buf);
    while (1)
    {
        nRead = read_file(&buf[streamPos], rdPos, spaceRemaining);
        if (nRead == 0)
        {
            // EOF reached, done
            break;
        }
        else if (nRead < 0)
        {
            log_message(LOG_ERR, "Error: reading from \"%s\" errno=%d\n", STORAGE_DATA_PATH, nRead);
            continue; // Continue reading data
        }
        else
        {
            // Write byte to socket
            nWrite = write(clientfd, &buf[streamPos], nRead);

            if (nWrite < 0)
            {
                log_message(LOG_ERR, "Error: writing to client socket errno=%d\n", nWrite);
                continue; // Continue reading data
            }

            log_message(LOG_DEBUG, "socket wr: %d bytes", nWrite);

            // Increment position into file
            rdPos += nWrite;

            if (nWrite == nRead)
            {
                // All data was saved, reset
                streamPos = 0;
                spaceRemaining = sizeof(buf);
            }
            else
            {
                // Not all data was written
                streamPos += nWrite;
                spaceRemaining -= nWrite;
            }
        }
    }

    // Close connection with client
    close(pTP->clientfd);
    log_message(LOG_INFO, "Closed connection with %s", inet_ntoa(pTP->clientAddr.sin_addr));
}