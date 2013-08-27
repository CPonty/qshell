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
#define SIGNAL_STOP SIGTERM

#define USAGE "Usage: qshell [FILE]\n"
#define WARN_CMDLINE_ARGS "qshell: command line input too long (>20 args)."\
" Ignoring extra arguments\n"
#define WARN_CMDLINE_CHARS "qshell: command line input too long"\
" (>128 characters). Ignoring extra characters\n"
#define WARN_CMDLINE_SYNTAX "qshell: unsupported command line syntax. "\
"Correct formatting:\n"\
"  command [arg1 arg2 ...] [< input_file] [> output_file] [&]\n"\
"  command [arg1 arg2 ...] [> output_file] [< input_file] [&]\n"\
"  command [arg1 ...] [< input_file] | command2 [arg1 ...] [> output_file]\n"

#define streq(p,q) (strcmp(p,q)==0)
#define fprintflush(fil,...) {fprintf(fil,__VA_ARGS__); fflush(fil);}
#define fprinterr(...) {fprintf(stderr,__VA_ARGS__); fflush(stderr);}
#define fprintout(...) {fprintf(stdout,__VA_ARGS__); fflush(stdout);}
#define fnerror(fnname) {fprintf(stderr, "%s() error : %s\n",fnname, strerror(errno)); fflush(stderr);}

#define DEBUG 2

/* TYPES */

typedef unsigned char bool;

/* GLOBALS */

FILE * input; // pointer to input stream (stdin or a file)
sigset_t sigList; // list of signals to block
struct sigaction storedSigActions[4]; // keep signal actions to restore
/* 0: SIGINT
 * 1: SIGCHLD
 * 2: SIGTERM
 * 3: SIGPIPE
 */
bool ctrlc = 0; // flag to indicate SIGINT/CTRL-C pressed
int fgPid[2] = {-1}; // PID of the currently running foreground process(es)
int * bgPids; // PIDs for currently running background process(es)
int bgPidBufsize = BUFFER_INCREMENT; // length of bgPid buffer; must be >0

/* FUNCTION PROTOTYPES */

/* top level */
int main (int argc, char * argv[]);
void parse_args (int argc, char * argv[]);
void init ();
void stop ();

/* input parsing */
void input_load (char * fname);
void input_read ();
void input_parse (int argc, char * argv[]);
void input_exec (int arg1c, char * arg1v[], int arg2c, char * arg2v[],
	char * inFname, char * outFname, bool background);

/* signal handling */
void sig_do_int (int status);
void sig_do_child (int status);
void sig_do_shutdown (int status);
void sig_do_pipe (int status);
void sig_setup ();
void sig_cancel ();
void sig_block (int signal);
void sig_unblock (int signal);

/* process handling */
void proc_set_stream (char * fname, char * sname, char * mode, FILE * stream);
void proc_reap (int waitPid);
void proc_do_reaped (int pid, int status);
void proc_killall ();
void proc_background_add (int pid);

/* */

#endif
