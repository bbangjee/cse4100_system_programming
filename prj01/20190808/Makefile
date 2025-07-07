CC = gcc
CFLAGS = -g -Wall  
LDFLAGS =

SRCS = main.c bitmap.c debug.c hash.c hex_dump.c list.c
OBJS = $(SRCS:.c=.o)
DEPS = bitmap.h debug.h hash.h hex_dump.h list.h round.h limits.h   #

TARGET = testlib

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
