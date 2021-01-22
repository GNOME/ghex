# TEMPORARY MAKEFILE FOR TESTING PURPOSES ONLY

DEBUG=-g -DENABLE_DEBUG
CC=gcc
GTK_CFLAGS=$(shell pkg-config --libs --cflags gtk4)
CFLAGS=-Wall -Wextra -Werror=implicit -std=c11 -pedantic \
       -Wno-unused-variable -Wno-unused-parameter \
       $(GTK_CFLAGS) \
       -I../build -I. \
       -DHAVE_CONFIG_H \
       $(DEBUG)

.PHONY: clean compile-resources

STUB: gtkhex.o hex-document.o ghex-application-window.o hex-dialog.o findreplace.o chartable.o converter.o resources.o configuration.o preferences.o common-ui.o print.o

compile-resources:
	glib-compile-resources ghex.gresource.xml --target=resources.c --generate-source

resources.o:
	make compile-resources
	$(CC) resources.c -c -o resources.o $(CFLAGS)

clean:
	rm -f STUB *.o resources.c
