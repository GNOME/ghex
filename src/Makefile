CC = gcc
CFLAGS = -g -I /usr/local/include/libgnomesupport
LIBDIRS = -L /usr/X11/lib -L /usr/local/lib
LIBS =  -lgtk -lgdk -lX11 -lgnome -lgnomeui -lgnomesupport -lc -lglib -lXext -lm -lintl -lICE -lSM


ghex: main.o ui.o io.o gnome-support.o gtkhex.o callbacks.o config.o
	${CC} ${CFLAGS} -o ghex main.o ui.o callbacks.o io.o \
	gnome-support.o config.o gtkhex.o ${LIBDIRS} ${LIBS}

main.o: main.c gtkhex.h ghex.h gnome-support.h
	${CC} ${CFLAGS} -o main.o -c main.c

ui.o: ui.c callbacks.h gtkhex.h ghex.h
	${CC} ${CFLAGS} -o ui.o -c ui.c

callbacks.o: callbacks.c callbacks.h ghex.h gtkhex.h
	${CC} ${CFLAGS} -o callbacks.o -c callbacks.c

io.o: io.c gtkhex.h ghex.h
	${CC} ${CFLAGS} -o io.o -c io.c

config.o: config.c ghex.h
	${CC} ${CFLAGS} -o config.o -c config.c

gnome-support.o: gnome-support.c gnome-support.h ghex.h
	${CC} ${CFLAGS} -o gnome-support.o -c gnome-support.c

gtkhex.o: gtkhex.c gtkhex.h
	${CC} ${CFLAGS} -o gtkhex.o -c gtkhex.c

clean:
	rm -f *.o core ghex

