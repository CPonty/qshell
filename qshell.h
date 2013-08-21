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
#define WARN_CMDLINE_ARGS "qshell: command line input too long (>20 args)."\
" Ignoring extra arguments\n"
#define WARN_CMDLINE_CHARS "qshell: command line input too long"\
" (>128 characters). Ignoring extra characters\n"
#define WARN_CMDLINE_SYNTAX "qshell: unrecognised command line syntax. "\
"Correct formatting:\n"\
"  command [arg1 arg2 ...] [< input_file] [> output_file] [&]\n"\
"  command [arg1 arg2 ...] [> output_file] [< input_file] [&]\n"\
"  command [arg1 ...] [< input_file] | command2 [arg1 ...] [> output_file]\n"

#define streq(p,q) (strcmp(p,q)==0)
#define fprintflush(fil,...) {fprintf(fil,__VA_ARGS__); fflush(fil);}

#define DEBUG 2

/* TYPES */

typedef unsigned char bool;

/* GLOBALS */

FILE * input; // pointer to input stream (stdin or a file)
sigset_t sigList; // list of signals to block
struct sigaction storedSigActions[4]; // keep signal actions to restore
bool ctrlc = 0;

/* FUNCTION PROTOTYPES */

/* top level */
int main (int argc, char * argv[]);
void parse_args (int argc, char * argv[]);
void init ();
void stop ();

/* input parsing */
void read_input ();
void parse_input (int argc, char * argv[]);
void load_input (char * fname);

/* signal handling */
void sig_do_int (int status);
void sig_do_child (int status);
void sig_do_shutdown (int status);
void sig_do_pipe (int status);
void sig_setup ();
void sig_cancel ();
void sig_block ();
void sig_unblock ();

/* process handling */
void proc_parent_forked (int fdc, int fdv[], int argc, char *argv[], int pid);
void proc_child_forked (int fdc, int fdv[], int argc, char *argv[], int pid);
void proc_killall ();


void sys_message (int msgCode);
/* */

#endif
