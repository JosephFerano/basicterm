P=bt
OBJECTS=
CFLAGS=-g -Wall -Wextra -pedantic -O0
LDLIBS=
CC=gcc
RM=rm -vf

bt: clean
	$(CC) $(CFLAGS) -lX11 $(P).c -o $(P)

clean:
	$(RM) $(P)

run:bt
	./$(P)

all:$(P)
