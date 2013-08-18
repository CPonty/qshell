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
#define DEBUG 0

/* TYPES */

typedef unsigned char bool;

/* GLOBALS */

FILE * input; 

/* FUNCTION PROTOTYPES */

int main (int argc, char * argv[]);

void init ();
void parse_args (int argc, char * argv[]);
void load_stdin (char * fname);

void sys_message (int msgCode);
/* */

#endif
