P=bt
CFLAGS=-g -Wall -Wextra -pedantic -O0
CC=gcc
RM=rm -vf

bt: clean
	$(CC) $(CFLAGS) -lraylib $(P).c -o $(P)

clean:
	$(RM) $(P)

run:bt
	./$(P)

all:$(P)
