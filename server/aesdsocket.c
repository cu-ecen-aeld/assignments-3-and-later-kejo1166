/**
 * @file aesdsocket.c
 * @author Kenneth A. Jones
 * @date 2022-02-05
 * 
 * @brief 
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

// ============================================================================
// PRIVATE MACROS AND DEFINES
// ============================================================================

#define DEBUG

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

// ============================================================================
// STATIC VARIABLES
// ============================================================================

static int serverfd = -1;
static int clientfd = -1;
static struct addrinfo *pServerInfo;

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
static int create_storage_file(void);

/**
 * @brief Save data to storage file
 * 
 * @param pBuf - Pointer stream of data
 * @param size - size of stream
 * @return int - 0 upon success; otherwise, error number
 */
static int save_to_storage_file(char *pBuf, ssize_t size);

/**
 * @brief Read from storage file
 * 
 * @param pBuf -Pointer to buffer
 * @param offset - Byte offset into file
 * @param maxSize - Maximum read size
 * @return int 
 */
static int read_from_storage_file(char *pBuf, int offset, ssize_t maxSize);

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

// ============================================================================
// GLOBAL FUNCTIONS
// ============================================================================

int main(int argc, char **argv)
{
    struct addrinfo hints;
    int status;

    // Create logger
    openlog(NULL, 0, LOG_USER);

    // Configure signal interrupts
    sig_t result = signal(SIGINT, sig_handler);
    if (result == SIG_ERR)
    {
        log_message(LOG_ERR, "Error: could not register SIGINT errno=%d\n", result);
        goto error;
    }

    result = signal(SIGTERM, sig_handler);
    if (result == SIG_ERR)
    {
        log_message(LOG_ERR, "Error: could not register SIGTERM errno=%d\n", result);
        goto error;
    }

    // Create storage file
    status = create_storage_file();
    if (status != 0)
    {
        log_message(LOG_ERR, "Error: creating storeage file errno=%d\n", status);
        goto error;
    }

    // Clear data structure
    memset(&hints, 0, sizeof(hints));

    // Get server info for IP address
    hints.ai_family = FAMILY;
    hints.ai_socktype = SOCKET_TYPE;
    status = getaddrinfo(NULL, PORT, &hints, &pServerInfo);
    if (status != 0)
    {
        log_message(LOG_ERR, "Error: getaddrinfo() %s\n", gai_strerror(status));
        goto error;
    }

    // Open socket connection
    serverfd = socket(FAMILY, SOCKET_TYPE, 0);
    if (serverfd < 0)
    {
        log_message(LOG_ERR, "Error: opening socket, errno=%d\n", errno);
        goto error;
    }

    // Bind device address to socket
    if (bind(serverfd, pServerInfo->ai_addr, pServerInfo->ai_addrlen) < 0)
    {
        log_message(LOG_ERR, "Error: binding socket errno=%d\n", errno);
        goto error;
    }

    // Listen for connection
    if (listen(serverfd, MAX_CONNECTIONS < 0))
    {
        log_message(LOG_ERR, "Error: listening for connection errno=%d\n", errno);
        goto error;
    }

    // Listen for connection forever
    while (1)
    {
        struct sockaddr_in clientAddr;                 // Specified address of accepted client
        socklen_t clientAddrSize = sizeof(clientAddr); // Size of client address
        char buf[1024];
        ssize_t nRead;
        ssize_t nWrite;
        ssize_t spaceRemaining = sizeof(buf);
        int streamPos = 0;

        log_message(LOG_INFO, "Listening for clients on port %s ...", PORT);

        // Accept incoming connections.  accept() will block until connection
        clientfd = accept(serverfd, (struct sockaddr *)&clientAddr, &clientAddrSize);
        if (clientfd < 0)
        {
            log_message(LOG_ERR, "Error: failed to accept client errno=%d\n", errno);
            goto error;
        }

        log_message(LOG_INFO, "Accepted connection from %s", inet_ntoa(clientAddr.sin_addr));

        // Read data from socket
        while (1)
        {
            nRead = read(clientfd, &buf[streamPos], spaceRemaining);
            if (nRead < 0)
            {
                log_message(LOG_ERR, "Error: reading from socket errno=%d\n", errno);
                continue; // Continue reading data
            }

            // Save data received from client
            status = save_to_storage_file(&buf[streamPos], nRead);
            if (status < 0)
            {
                log_message(LOG_ERR, "Error: saving data to file errno=%d\n", status);
                continue; // Continue reading data
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

            // Once \n has been received, send storage file to client
            if (strchr(buf, '\n'))
                break;
        }

        // Write data to socket
        int rdPos = 0;
        streamPos = 0;
        spaceRemaining = sizeof(buf);
        while (1)
        {
            nRead = read_from_storage_file(&buf[streamPos], rdPos, spaceRemaining);
            if (nRead == 0)
                break; // Done reading file
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
        close(clientfd);
        log_message(LOG_INFO, "Closed connection with %s", inet_ntoa(clientAddr.sin_addr));
    }

    cleanup();
    return 0;

error:
    cleanup();
    return -1;
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

int create_storage_file(void)
{
    // File does not exist, so it must be created.
    int fd = creat(STORAGE_DATA_PATH, 0755);
    if (fd < 0)
        return errno;

    close(fd);
    return 0;
}

int save_to_storage_file(char *pBuf, ssize_t size)
{
    int fd = open(STORAGE_DATA_PATH, O_WRONLY);
    if (fd < 0)
        return errno;

    // Move to end of file
    lseek(fd, 0, SEEK_END);

    // Write data to file
    int count = write(fd, pBuf, size);

    return (count == size) ? 0 : 1;
}

int read_from_storage_file(char *pBuf, int offset, ssize_t maxSize)
{
    // Open file
    int fd = open(STORAGE_DATA_PATH, O_RDONLY);
    if (fd < 0)
        return -1;

    // Move to position
    lseek(fd, offset, SEEK_SET);

    return read(fd, pBuf, maxSize);
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

     // Close sys log
    closelog();

    // Remove storage file
    unlink(STORAGE_DATA_PATH);
}

void sig_handler(int signo)
{
    log_message(LOG_INFO, "Caught signal %d, exiting ...", signo);
    cleanup();
    exit(0);
}