CC = gcc
CFLAGS = -lcrypt -pthread
myproxy: myproxy.c color.h
	${CC} -o myproxy myproxy.c ${CFLAGS}
