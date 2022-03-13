#! /bin/sh


# System V startup script that starts aesdsocket.
# Reference: Linux System Initialization video and Mastering Embedded Linux Programming Chapter 10

case "$1" in
    start)	
        echo "Starting aesdsocket"
        start-stop-daemon -S -n aesdsocket -a /usr/bin/aesdsocket -- -d
        ;;
    stop)
        echo "Stopping aesdsocket" 
        start-stop-daemon -K -n aesdsocket
        ;;
    *)
        echo "Usage: $0 {start|stop}" 
        exit 1 
esac

exit 0