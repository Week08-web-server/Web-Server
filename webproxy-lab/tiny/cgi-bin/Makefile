CC = gcc
CFLAGS = -O2 -Wall -I ..

all: adder

adder: adder.c ../logger.c ../logger.h
	$(CC) $(CFLAGS) -o adder adder.c ../logger.c

clean:
	rm -f adder *~
