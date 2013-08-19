//
//  qshell.h
//

#ifndef QSHELL_H
#define QSHELL_H

/* INCLUDES */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>

/* MACROS */

#define BUFFER_INCREMENT 128
#define USAGE "Usage: qshell [FILE]\n"
#define WARN_CMDLINE_ARGS "Warning: command line input too long (>20 args)."\
" Ignoring extra arguments\n"
#define WARN_CMDLINE_CHARS "Warning: command line input too long"\
" (>128 characters). Ignoring extra characters\n"
#define DEBUG 2

/* TYPES */

typedef unsigned char bool;

/* GLOBALS */

FILE * input; 

/* FUNCTION PROTOTYPES */

int main (int argc, char * argv[]);

void init ();
void stop ();
void parse_args (int argc, char * argv[]);

void read_input ();
void parse_input (int argc, char * argv[]);
void load_input (char * fname);

void sys_message (int msgCode);
/* */

#endif
