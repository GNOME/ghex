# TEMPORARY MAKEFILE FOR TESTING PURPOSES ONLY

DEBUG=-g -DENABLE_DEBUG
CC=gcc
GTK_CFLAGS=$(shell pkg-config --libs --cflags gtk4)
CFLAGS=-Wall -Wextra -std=c11 -pedantic \
       $(GTK_CFLAGS) \
       -I../build -I. \
       $(DEBUG)

STUB: gtkhex.o hex-document.o

clean:
	rm -f STUB *.o

.PHONY: clean
