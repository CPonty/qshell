//
//  qshell.c
//

#include "qshell.h"

/* ------------------------------------------------------------------------- */
/* Top level */

int main (int argc, char * argv[]) {
	#if DEBUG>1
	fprinterr(" === QSHELL-%d === \n\n", getpid());
	#endif
	
	init();
	parse_args(argc, argv);	
	return 0;
}

void init () {
	/*
	 * Setup: signal handling, memory allocation
	 */
	input = stdin;
	sig_setup();
	bgPids = (int *)malloc(sizeof(int)*bgPidBufsize);
}

void stop () {
	/*
	 * About to close shell
	 *  - stop background processes
	 *  - free memory
	 *  - exit with status 0 (OK)
	 */
	proc_reap(-1);
	proc_killall();
	free(bgPids);
	#if DEBUG>0
	fprinterr("\nExiting, status 0\n");
	#else
	fprinterr("\n");
	#endif
	exit(0);
}

void parse_args (int argc, char * argv[]) {
	/*
	 * Decide on action to take at startup
	 */
	if (argc==1) {
		/* No parameters - start interactive input */
		input_read();	
	} else if (argc==2) {
		/* One parameter - open as file to read commands from */
		input_load(argv[1]);
		input_read();
	} else {
		/* 2+ paramters - not supported */
		printf(USAGE);
	}
}

/* ------------------------------------------------------------------------- */
/*input handling */

void input_load (char * fname) {
	/*
	 * Set the input stream to a file pointer to FNAME
	 */
	input = fopen(fname,"r");
	if (input==NULL) {
		fprintf(stderr, "Unable to open commands file '%s': %s\n",
			fname, strerror(errno));
		exit(1);
	}
}

void input_read () {
	/*
	 * Main event loop to collect commands from input lines
	 */
	char c, strbuf[256], *bufp;
	char *argv[20], **argp;
	int charc, argc;

	fd_set fds;
	struct timeval timeout;
	int readyStreams;

	while (1) {
		ctrlc=0;

		/* 1. Print prompt (interactive mode) */
		if (input==stdin) {
			if (getcwd(strbuf,sizeof(strbuf))==NULL) {
				fnerror("getcwd");
				exit(2);
			}
			fprintout("[%s]? ",strbuf);
		}

		/* 2. Read from input stream.
		 */
		charc=0;
		c='\0';
		// before actually reading anything, select() on input
		//   this will allow us to pick up ctrl-c (SIGINT)
		//   without getting stuck in the blocking fgetc() loop
		while (!ctrlc) {
			FD_ZERO(&fds);
			FD_SET(fileno(input), &fds);
			timeout.tv_sec = 0;
			// block for some ms at a time
			timeout.tv_usec = 10000;
			readyStreams = select(fileno(input)+1, &fds, NULL, 
				NULL, &timeout); 
			// ignore signals interrupting our system call
			if (readyStreams<0) {
				if (errno == EINTR) continue;	
				fnerror("select");
				exit(2);
			}
			else if (readyStreams==0) {
				// not ready
				//// fprinterr(".");
			} else {
				// ready
				break;
			}
			
		}

		if (!ctrlc) {
			// read up to 128 chars. Split on EoF/NL/comment(#)
			//   yes, I know I could have used readline(). 
			//   I wanted to try some new things!
			while ((c=fgetc(input))!='\n' && c!='#' && \
				!feof(input) && (charc<128)) {
				strbuf[charc++]=c;
			}
			strbuf[charc]='\0';
			// handle ctrl-c
			if (ctrlc) {
				fprintout("control-c!");
			}
			// flush characters until the next newline
			if (charc==128 || c=='#') {
				while (fgetc(input)!='\n' && !feof(input));
			}
			#if DEBUG>0
			if (input!=stdin) {
				fprintout("%s\n", strbuf);
			}
			#endif
			// warning for too many characters
			if (charc==128) {
				fprinterr(WARN_CMDLINE_CHARS);
			}
	
			/* 3. Separate arguments. Max 20 arguments. */
			for (int i=0; i<20; i++) {
				argv[i]=NULL;
			}
			argc=0;
			argp=argv;
			bufp=strbuf;
			for (; (*argp=strsep(&bufp, " "))!=NULL; ) {
				if (**argp!='\0') {
					argp++;
					argc++;
					if (argp>&argv[20]) 
						break;
				}	
			}
			//warning for too many arguments
			if (argc>20) {
				argc=20;
				fprinterr(WARN_CMDLINE_ARGS);
			}
	
			/* 4. Process input */
			if (argc>0) {
				input_parse(argc, argv);
			}
		}	
		// flush characters until the next newline
		if (charc==128 || c=='#') {
			while (fgetc(input)!='\n' && !feof(input));
		}


		/* 5. Print background process output/termination */
		proc_reap(-1);

		/* 6. STOP before we prompt again - is it EoF?
		 *     - For interactive mode, this is any zero-char read
		 *     - For file mode, we want to stop anytime we hit feof
		 */
		if (((charc==0) && (c!='\n') && (c!='#') && !ctrlc) || \
		    (input!=stdin && feof(input))) {
			stop();
		}

	}
}

void input_parse (int argc, char * argv[]) {
	/*
	 * Handle a list of arguments from an input line
	 */
	int pipePos=-1, inDirectPos=-1, outDirectPos=-1, backPos=-1;
	int command1Pos=0, command2Pos=-1, command1End=-1, command2End=-1;
	int consecArgc=0;
	char * argPtr;

	#if DEBUG>1
	fprintf(stderr, "argc=%d\n", argc);
	for (int i=0; i<argc; i++) {
		fprinterr("arg[%d]=%s\n", i, argv[i]);
	}
	#endif
	// 1. check for inbuilt commands "cd" and "exit"
	if (streq(argv[0], "exit")) {
		stop();
	} else if (streq(argv[0], "cd")) {
		// >1 args: a path was provided (ignore extra args)
		if (argc>1 && !streq(argv[1],"~")) {
			argPtr = argv[1];			
		// 1 args: no path provided, go to HOME
		} else {
			if ((argPtr = getenv("HOME"))==NULL) {
				fnerror("getenv");
			}
		}
		// chdir
		if (chdir(argPtr)<0) {
			fnerror("chdir");
		}
		return;
	}

	// 2. not inbuilt: loop through the arguments for syntax validation`
	else for (int i=0; i<argc; i++) {
		// pipe (|)
		if streq(argv[i], "|") {
			// enforce correct location
			if (pipePos>=0 || i==argc-1 || consecArgc==0 ||\
				outDirectPos>=0) {
				fprintout(WARN_CMDLINE_SYNTAX);
				return;
			}
			// log info about argument structure
			if (command2Pos==-1 && command1End==-1) {
				command1End=i-1;
			}	
			consecArgc=0;
			command2Pos=i+1;
			pipePos=i;
		}
		// input redirect (<)
		else if streq(argv[i], "<") {
			// enforce correct location
                        if (inDirectPos>=0 || i==argc-1 || consecArgc==0 ||\
				pipePos>=0)  {
                                fprintout(WARN_CMDLINE_SYNTAX);
                                return;
                        }
			// log info about argument structure
			if (command2Pos==-1 && command1End==-1) {
				command1End=i-1;
			}	
			consecArgc=0;
			consecArgc=0;
			inDirectPos=i;
		}
		// output redirect (>)
		else if streq(argv[i], ">") {
			// enforce correct location
                        if (outDirectPos>=0 || i==argc-1  || consecArgc==0) {
                                fprintout(WARN_CMDLINE_SYNTAX);
                                return;
                        }
			// log info about argument structure
			if (command2Pos==-1 && command1End==-1) {
				command1End=i-1;
			} else if (command2End==-1) {
				command2End=i-1;
			}	
			consecArgc=0;
			consecArgc=0;
			outDirectPos=i;

		}
		// background (&)
		else if streq(argv[i], "&") {
			// enforce correct location
                        if (backPos>=0 || i<argc-1 || consecArgc==0) {
                                fprintout(WARN_CMDLINE_SYNTAX);
                                return;
                        }
			// log info about argument structure
			if (command2Pos==-1 && command1End==-1) {
				command1End=i-1;
			} else if (command2End==-1) {
				command2End=i-1;
			}
			consecArgc=0;
			consecArgc=0;
			backPos=i;

		}
		// other: command/arg
		else {
			consecArgc++;
			// input/output redirection takes only 1 arg
			if (consecArgc==2 && i>=2) {
				if (outDirectPos==i-2 || inDirectPos==i-2) {
					fprintout(
						WARN_CMDLINE_SYNTAX);
					return;
				}
			} 
		}
	}
	// Close off end of commands
	if (command1End==-1) {
		command1End=argc-1;
	}
	if (command2Pos>=0 && command2End==-1) {
		command2End=argc-1;
	}	

	// 3 execute!
	#if DEBUG>1
	fprinterr("exec com1=%d..%d com2=%d..%d in=%d "\
		"out=%d pipe=%d\n", command1Pos, command1End, 
		command2Pos, command2End, inDirectPos, outDirectPos,
		pipePos);
	#endif

	input_exec(command1End-command1Pos+1,
		&argv[command1Pos],
		(command2Pos==-1?0:command2End-command2Pos+1),
		(command2Pos==-1?NULL:&argv[command2Pos]),
		(inDirectPos==-1?NULL:argv[inDirectPos+1]),
		(outDirectPos==-1?NULL:argv[outDirectPos+1]),
		(backPos==-1?0:1)
	);
	return;
}

void input_exec (int arg1c, char * arg1v[], int arg2c, char * arg2v[],
        char * inFname, char * outFname, bool background) {
	/*
	 * Execute parsed/validated input commands
	 */
	int i;
	int status;
	int fdPipe[2]; //idx 0: read, 1: write
	char *exec1v[21], *exec2v[21];
	pid_t pid1=-1, pid2=-1;

	#if DEBUG>0
	fprinterr("exec\n");
	#endif
	// allocate null-terminated argument vectors
	for (i=0; i<arg1c+1; i++) {
		exec1v[i] = arg1v[i];
	}
	exec1v[i-1]=NULL;
	if (arg2c>0) {
		for (i=0; i<arg2c+1; i++) {
			exec2v[i] = arg2v[i];
		}
		exec2v[i-1]=NULL;
	}
	// create pipe(s) (command1->command2)
	if (arg2c) {
		pipe(fdPipe);
	}
	// block child handling while exec'ing
	sig_block(SIGCHLD);

	// fork and execute command 1
	if (!(pid1 = fork())) {
		// CHILD 1
		// 0. Cancel signal handlers from qshell
		sig_cancel();
		// 1. Setup input/output files (if applicable)
		//      background processes redirect std[in|out|err]
		//      to /dev/null unless otherwise specified
		if (background && inFname==NULL) {
			proc_set_stream("/dev/null", "stdin", "r", stdin);
		} else {
			proc_set_stream(inFname, "stdin", "r", stdin);
		}
		if (background && outFname==NULL) {
			proc_set_stream("/dev/null", "stdout", "w", stdout);
		} else if (!arg2c) {
			proc_set_stream(outFname, "stdout", "w", stdout);
		}
		if (background) {
			proc_set_stream("/dev/null", "stderr", "w", stderr);
		}
		// 2. Redirect pipe write end (if applicable)
		if (arg2c) {
			dup2(fdPipe[1], fileno(stdout));
			close(fdPipe[0]);
		}
		// 3. Exec. Error code will not run if exec succeeds
		execvp(exec1v[0], &exec1v[0]);
		fnerror("exec");
		exit(EXIT_FAILURE);
	}

	// fork and execute any piped command
	if (arg2c)
	if (!(pid2 = fork())) {
		// CHILD 2
		// 0. Cancel signal handlers from qshell
		sig_cancel();
		// 1. Setup output file (if applicable)
		proc_set_stream(outFname, "stdout", "w", stdout);
		// 2. Redirect pipe read end
		dup2(fdPipe[0], fileno(stdin));
		close(fdPipe[1]);
		// 3. Exec. Error code will not run if exec succeeds
		execvp(exec2v[0], &exec2v[0]);
		fnerror("exec");
		exit(EXIT_FAILURE);
	}

	// PARENT
	// 0. Close pipes
	if (arg2c) {
		close(fdPipe[0]);
		close(fdPipe[1]);
	}
	// 1. Foreground: store PID(s) and wait/reap 
	if (!background) {
		fgPid[0]=pid1;
		fgPid[1]=pid2;
		#if DEBUG>0
		fprinterr("FG Process : %d\n", pid1);
		if (arg2c) fprinterr("FG (Piped) Process : %d\n", pid2);
		#endif
		waitpid(fgPid[0], &status, 0);
		fgPid[0]=-1; 
		proc_do_reaped(pid1, status);
		if (arg2c) {
			waitpid(fgPid[1], &status, 0);
			fgPid[1]=-1; 
			proc_do_reaped(pid2, status);
		}
	} else {
		// 2. Background: ----------
		proc_background_add(pid1);
		#if DEBUG>0
		fprinterr("BG Process : %d\n", pid1);
		#endif
	}

	sig_unblock(SIGCHLD);
	#if DEBUG>0
	fprinterr("\\exec\n");
	#endif

}



/* ------------------------------------------------------------------------- */
/* process handling */

void proc_set_stream (char * fname, char * sname, char * mode, FILE * stream) {
	/*
	 * Replace stdin, stdout, stderr with files
	 */
	if (fname) {
		#if DEBUG>0
		fprinterr("%s file : %s\n", sname, fname);
		#endif
		if (!freopen(fname, mode, stream)) {
			fnerror("freopen");
			exit(EXIT_FAILURE);
		}
	}
}

void proc_reap (int waitPid) {
	/*
	 * Reap (clean up) child processes.
	 * waitPid specifies a certain process to wait for, -1 for all
	 */
	int pid, status;
	int childExists=0;

	
}

void proc_do_reaped(int pid, int status) {
	/*
	 * Handle a reaped process
	 */
	
}

void proc_killall () {
	/*
	 * Terminate all child processes (foreground/background) and reap
	 */

}

void proc_background_add (int pid) {
	/*
	 * Add a PID to the background process buffer.
	 *   grow buffer if needed
	 */
}


/* ------------------------------------------------------------------------- */
/* signal handling */

void sig_do_int (int status) {
	/*
	 * Interrupt on SIGINT
	 */
	#if DEBUG>0
	fprinterr("SIGINT(%d)\n", status);
	#else
	fprintout("\n");
	#endif
	ctrlc=1;

	// Kill currently running child (if any)
	for (int i=0; i<2; i++)
	if (fgPid[i]!=-1) {
		kill(fgPid[i], SIGNAL_STOP);
		#if DEBUG>0
		fprinterr("Terminating fg process %d : %d\n", i+1, fgPid[i]);
		#endif
	}
}

void sig_do_child (int status) {
        /*
         * Debug handler so we can see when children signal completion.
	 * We do reaping elsewhere (input loop, just before prompt)
         */
	#if DEBUG>0
	fprinterr("SIGCHLD(%d)\n", status);
	#endif
}

void sig_do_shutdown (int status) {
        /*
         * Interrupt on SIGTERM - gracefully shutdown
         */
	#if DEBUG>0
	fprinterr("SIGTERM(%d)\n", status);
	#endif
	stop();
}

void sig_do_pipe (int status) {
        /*
         * Debug handler in case we get sigpipe from stream writes/reads
         */
	#if DEBUG>0
	fprinterr("SIGPIPE(%d)\n", status);
	#endif
}

void sig_setup () {
        /*
         * Enable interrupts for signal handling
         */
	struct sigaction sa[4];
	
	// SIGINT (ctrl-c) kills active process
	sa[0].sa_handler = sig_do_int;
	sa[0].sa_flags = SA_RESTART;
	sigaction(SIGINT, &sa[0], &storedSigActions[0]);
	sa[1].sa_handler = sig_do_child;
	sa[1].sa_flags = SA_RESTART;
	sigaction(SIGCHLD, &sa[1], &storedSigActions[1]);
	sa[2].sa_handler = sig_do_shutdown;
	sa[2].sa_flags = SA_RESTART;
	sigaction(SIGTERM, &sa[2], &storedSigActions[2]);
	sa[3].sa_handler = sig_do_pipe;
	sa[3].sa_flags = SA_RESTART;
	sigaction(SIGPIPE, &sa[3], &storedSigActions[3]);
}

void sig_cancel () {
        /*
         * Restore signal handlers stored in storedSigActions
	 * Used by child processes
         */
	sigaction(SIGINT,  &storedSigActions[0], 0);
	sigaction(SIGCHLD, &storedSigActions[1], 0);
	sigaction(SIGTERM, &storedSigActions[2], 0);
	sigaction(SIGPIPE, &storedSigActions[3], 0);
}

void sig_block (int signal) {
        /*
         * Block handling for a signal type
         */
	sigemptyset(&sigList);
	sigaddset(&sigList, signal);
	sigprocmask(SIG_BLOCK, &sigList, NULL);
}

void sig_unblock (int signal) {
        /*
         * Unblock handling for a signal type
         */
	sigprocmask(SIG_UNBLOCK, &sigList, NULL);
	sigemptyset(&sigList);
}

/* ------------------------------------------------------------------------- */

