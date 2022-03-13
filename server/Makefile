# Reference: https://spin.atomicobject.com/2016/08/26/makefile-c-projects/ for assistance with make file.

SRCS = $(wildcard *.c)
OBJS = $(SRCS:.c=.o)

ifeq ($(CC),)
	CC = $(CROSS_COMPILE)gcc
endif

ifeq ($(CFLAGS),)
	CFLAGS = -g -Wall -Werror
endif 

ifeq ($(LDFLAGS),)
	LDFLAGS = -pthread -lrt
endif 

TARGET = aesdsocket
all: $(TARGET)
default: $(TARGET)

$(TARGET): $(SRCS) 
	$(CC) $(CFLAGS) $^ -o $@ $(INCLUDES) $(LDFLAGS)

.PHONY: clean
clean:
	rm -f $(OBJS) $(TARGET)