#
# Makefile for QShell
#

STUDENT_ID="s4234549"

DIST_FILES=qshell.c qshell.c qshell.1 qshell makefile LICENSE README.md
SRC_FILES=qshell.c
EXEC_FILE=qshell
TEST_FILE=scratch.sh
##MAN_FILE=qshell.1

QSHELL_SRC_DIR=./
DIST_DIR=./qshell-dist
##MAN_DIR=/usr/share/man/man1

CC=gcc
CFLAGS =-c -pedantic -Wall -Wextra -std=c99


clean: 
	@rm -fv *.o qshell

qshell.o: qshell.c
##	@echo "Compile: "${SRC_FILES}
	${CC} ${CFLAGS} ${SRC_FILES}

qshell: qshell.o
##	@echo "Link: "${EXEC_FILE}
	${CC} qshell.o -o ${EXEC_FILE}

build: qshell
	@echo "Binary: "${EXEC_FILE}

dist: clean build
	tar -cvzf ${STUDENT_ID}.tar.gz ${DIST_FILES}
	@echo "Distribution: "${STUDENT_ID}".tar.gz"

unzip: dist
	@rm -rf ${DIST_DIR}
	mkdir ${DIST_DIR}
	@cp ${STUDENT_ID}.tar.gz ${DIST_DIR}
	@cd ${DIST_DIR}
	tar -xvzf ${STUDENT_ID}.tar.gz
	@echo "Unzipped: "${STUDENT_ID}".tar.gz"

##	man: qshell.1
##		@cp ${MAN_FILE} ${MAN_DIR}
##		@echo "Man file "${MAN_FILE}" copied to "${MAN_DIR}

all: clean build

test: build
	@echo "Test: "${TEST_FILE}
	@./${TEST_FILE}

help:
	@echo -e "Targets:\n"\
	"  clean    delete objects, binaries\n"\
	"  build    compile qshell binary\n"\
	"  dist     create .tar.gz distributable\n"\
	"  unzip    extract .tar.gz distributable\n"\
	"  all      clean, build\n"\
	"  test     run tests\n"\
	"  help     show this help text"

#
# make -C ${QSHELL_SRC_DIR} #run make command in other directory
# 
# DON'T FORGET - makefiles are basically bash scripts with "targets:" added
