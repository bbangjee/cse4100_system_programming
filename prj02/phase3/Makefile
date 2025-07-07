CC = gcc
CFLAGS = -Og
LDLIBS = -lpthread

PROG = myshell
SRCS = myshell.c
OBJS = $(SRCS:.c=.o)

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(LDLIBS) -o $(PROG)

%.o: %.c myshell.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJS) $(PROG)
