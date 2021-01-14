# TEMPORARY MAKEFILE FOR TESTING PURPOSES ONLY

DEBUG=-g -DENABLE_DEBUG
CC=gcc
GTK_CFLAGS=$(shell pkg-config --libs --cflags gtk4)
CFLAGS=-Wall -Wextra -std=c11 -pedantic \
       $(GTK_CFLAGS) \
       -I../build -I. \
       $(DEBUG)

.PHONY: clean compile-resources

STUB: gtkhex.o hex-document.o ghex-application-window.o hex-dialog.o findreplace.o resources.o

compile-resources:
	glib-compile-resources ghex.gresource.xml --target=resources.c --generate-source

resources.o:
	make compile-resources
	$(CC) resources.c -c -o resources.o $(CFLAGS)

clean:
	rm -f STUB *.o resources.c
