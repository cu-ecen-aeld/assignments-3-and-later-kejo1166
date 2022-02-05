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

/**
 * @brief Print message to syslog and terminal depending on configuation
 * 
 * @param logType - Syslog error type
 * @param __fmt - Formatted string
 * @param ... 
 */
static void log_message(int logType, const char *__fmt, ...);

// ============================================================================
// GLOBAL FUNCTIONS
// ============================================================================

int main(int argc, char **argv)
{
    struct addrinfo hints;
    struct addrinfo *pServerInfo;
    int serverfd = -1;

    // Create logger
    openlog(NULL, 0, LOG_USER);

    // Clear data structure
    memset(&hints, 0, sizeof(hints));

    // Get server info for IP address
    hints.ai_family = FAMILY;
    hints.ai_socktype = SOCKET_TYPE;
    int status = getaddrinfo(NULL, PORT, &hints, &pServerInfo);
    if (status != 0)
    {
        log_message(LOG_ERR, "Error: getaddrinfo() %s\n", gai_strerror(status));
        goto socketerror;
    }

    // Open socket connection
    serverfd = socket(FAMILY, SOCKET_TYPE, 0);
    if (serverfd < 0)
    {
        log_message(LOG_ERR, "Error: opening socket, errno=%d\n", errno);
        goto socketerror;
    }

    // Bind device address to socket
    if (bind(serverfd, pServerInfo->ai_addr, pServerInfo->ai_addrlen) < 0)
    {
        log_message(LOG_ERR, "Error: binding socket errno=%d\n", errno);
        goto socketerror;
    }

    // Listen for connection
    if (listen(serverfd, MAX_CONNECTIONS < 0))
    {
        log_message(LOG_ERR, "Error: listening for connection errno=%d\n", errno);
        goto socketerror;
    }

    log_message(LOG_INFO, "Listening for clients on port %s ...", PORT);

    // Listen for connection forever
    while (1)
    {
        struct sockaddr_in clientAddr;            // Specified address of accepted client
        socklen_t clientAddrSize = sizeof(clientAddr); // Size of client address
        int clientfd;

        // Accept incoming connections.  This function will block until connection
        clientfd = accept(serverfd, (struct sockaddr *)&clientAddr, &clientAddrSize);
        if(clientfd < 0)
        {
            log_message(LOG_ERR, "Error: failed to accept client errno=%d\n", errno);
            goto socketerror;
        }

        log_message(LOG_INFO, "Accepted connection from %s", inet_ntoa(clientAddr.sin_addr));
         
        // Close connection with client 
        close(clientfd);
        log_message(LOG_INFO, "Closed connection with %s", inet_ntoa(clientAddr.sin_addr));
    }

    // Close socket
    close(serverfd);

    // Free allocated address info
    if (pServerInfo)
        freeaddrinfo(pServerInfo);

    // Close sys log
    closelog();
    return 0;

socketerror:
    if (serverfd > 0)
        close(serverfd);
    closelog(); // Close sys log
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