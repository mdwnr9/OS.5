CC = gcc
CFLAGS = -g -Wall -I.

oss : oss.c 
	$(CC) -std=c99 -o oss oss.c

EXECS = oss 
all: $(EXECS)

clean:
	rm -f *.o $(EXECS)
