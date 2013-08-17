#
# Makefile for QShell
#

STUDENT_ID="s4234549"

QSHELL_SRC_DIR=./

DIST_FILES="qshell qshell.c qshell.h qshell.1 makefile"
MAN_FILE="qshell.1"
MAN_DIR=/usr/share/man/man1

CC=gcc
CFLAGS =-c -pedantic -Wall -Wextra -std=c99

#
#

clean: 
	rm -f *.o qshell

qshell.o: qshell.c qshell.h
	${CC} ${CFLAGS} qshell.c

qshell: qshell.o
	${CC} qshell.o -o qshell

build: qshell

#
#

dist: clean build
	tar -czf ${STUDENT_ID}".tar.gz" ${DIST_FILES} &&\
	echo "Created "${STUDENT_ID}".tar.gz"

man: qshell.1
	cp ${MAN_FILE} ${MAN_DIR}
	echo "Man file "${MAN_FILE}" copied to "${MAN_DIR}

all: clean build man

#
#

test: build
	#

#
# make -C ${QSHELL_SRC_DIR} #run make command in other directory
# 
# DON'T FORGET - makefiles are basically bash scripts with "targets:" added
