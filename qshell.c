//
//  qshell.c
//

#include "qshell.h"

/* ------------------------------------------------------------------------- */
/* Top level */

int main (int argc, char * argv[]) {
	init();
	parse_args(argc, argv);	
	return 0;
}

void init () {
	/*
	 * Setup: signal handling, initial values for global variables
	 */
	input = stdin;
	sig_setup();
	#if DEBUG>1
	fprintflush(stderr, " === QSHELL-%d === \n\n", getpid());
	#endif
}

void stop () {
	/*
	 * About to close shell
	 *  - stop background processes
	 *  - free memory
	 *  - exit with status 0 (OK)
	 */
	#if DEBUG>0
	fprintflush(stderr, "\nExiting, status 0\n");
	#else
	fprintflush(stderr, "\n");
	#endif
	exit(0);
}

void parse_args (int argc, char * argv[]) {
	/*
	 * Decide on action to take at startup
	 */
	if (argc==1) {
		/* No parameters - start interactive input */
		read_input();	
	} else if (argc==2) {
		/* One parameter - open as file to read commands from */
		load_input(argv[1]);
		read_input();
	} else {
		/* 2+ paramters - not supported */
		printf(USAGE);
	}
}

/* ------------------------------------------------------------------------- */
/*input parsing  */

void read_input () {
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
				fprintflush(stderr, "getcwd() error : %s\n", 
					strerror(errno));
				exit(2);
			}
			fprintflush(stdout, "[%s]? ",strbuf);
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
			// block for 1ms at a time
			timeout.tv_usec = 1000;
			readyStreams = select(fileno(input)+1, &fds, NULL, 
				NULL, &timeout); 
			// ignore signals interrupting our system call
			if (readyStreams<0) {
				if (errno == EINTR) continue;	
				fprintflush(stderr, "select() error : %s\n", 
					strerror(errno));
				exit(2);
			}
			if (readyStreams==0) {
				// not ready
				//// fprintflush(stderr,".");
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
				fprintflush(stdout, "control-c!");
			}
			// flush characters until the next newline
			if (charc==128 || c=='#') {
				while (fgetc(input)!='\n' && !feof(input));
			}
			#if DEBUG>0
			if (input!=stdin) {
				fprintflush(stdout, "%s\n", strbuf);
			}
			#endif
			// warning for too many characters
			if (charc==128) {
				fprintflush(stderr, WARN_CMDLINE_CHARS);
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
				fprintflush(stderr, WARN_CMDLINE_ARGS);
			}
	
			/* 4. Process input */
			if (argc>0) {
				parse_input(argc, argv);
			}
		}	
		// flush characters until the next newline
		if (charc==128 || c=='#') {
			while (fgetc(input)!='\n' && !feof(input));
		}


		/* 5. Print background process output/termination
		 *
		 */

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

void parse_input (int argc, char * argv[]) {
	/*
	 * Handle a list of arguments from an input line
	 */
	int pipePos=-1, inDirectPos=-1, outDirectPos=-1, backPos=-1;
	int command1Pos=0, command2Pos=-1, command1End=-1, command2End=-1;
	int consecArgc=0;
	bool valid=1;
	char * argPtr;

	#if DEBUG>1
	fprintf(stderr, "argc=%d\n", argc);
	for (int i=0; i<argc; i++) {
		fprintflush(stderr, "arg[%d]=%s\n", i, argv[i]);
	}
	#endif
	// 1. check for inbuilt commands "cd" and "exit"
	if (streq(argv[0], "exit")) {
		stop();
	} else if (streq(argv[0], "cd")) {
		// >1 args: a path was provided (ignore extra args)
		if (argc>1) {
			argPtr = argv[1];			
		// 1 args: no path provided, go to HOME
		} else {
			if ((argPtr = getenv("HOME"))==NULL) {
				fprintflush(stderr, "getenv() error : %s\n",
					strerror(errno));
			}
		}
		// chdir
		if (chdir(argPtr)<0) {
			fprintflush(stderr, "chdir() error : %s\n",
				strerror(errno));
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
				fprintflush(stdout, WARN_CMDLINE_SYNTAX);
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
                                fprintflush(stdout, WARN_CMDLINE_SYNTAX);
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
                                fprintflush(stdout, WARN_CMDLINE_SYNTAX);
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
                                fprintflush(stdout, WARN_CMDLINE_SYNTAX);
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
					fprintflush(stdout, 
						WARN_CMDLINE_SYNTAX);
					return;
				}
			} 
			// Still need to detect ends of commands
			//  - use characters <>|& above
			//
			if (0) {
			if (consecArgc==1 && i>=2) {
				if (command1Pos>=0 && command2Pos==-1 &&\
					command1End==-1) {
					command1End=i-2;
				} else if (command2Pos>=0 && command2End==-1) {
					command2End=i-2;
				}
			}
			if (consecArgc>=2) {
				if (command1Pos==-1) {
					command1Pos=i-1;
				} else if (command2Pos==-1 && command1End>0) {
					command2Pos=i-1;
				} else {
				}
			} }
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
	fprintflush(stderr, "exec com1=%d..%d com2=%d..%d in=%d "\
		"out=%d pipe=%d\n", command1Pos, command1End, 
		command2Pos, command2End, inDirectPos, outDirectPos,
		pipePos);
	#endif
	fprintflush(stderr, "exec\n");
	return;
}

void load_input (char * fname) {
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


/* ------------------------------------------------------------------------- */
/* process handling */

void proc_parent_forked (int fdc, int fdv[], int argc, char *argv[], int pid) {
        /*  
         * >
         */
     
}



void proc_child_forked (int fdc, int fdv[], int argc, char *argv[], int pid) {
        /*  
         * >
         */
     
}

void proc_killall () {
	/*
	 *
	 */

}


/* ------------------------------------------------------------------------- */
/* signal handling */

void sig_do_int (int status) {
	/*
	 * Interrupt on SIGINT
	 */
	#if DEBUG>0
	fprintflush(stderr, "SIGINT\n");
	#else
	fprintflush(stdout, "\n");
	#endif
	ctrlc=1;

	// Kill currently running child (if any)
	//
	//
}

void sig_do_child (int status) {
        /*
         * >
         */
    
}

void sig_do_shutdown (int status) {
        /*
         * >
         */
    
}

void sig_do_pipe (int status) {
        /*
         * >
         */
    
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
}

void sig_cancel () {
        /*
         * >
         */
    
}

void sig_block () {
        /*
         * >
         */
    
}

void sig_unblock () {
        /*
         * >
         */
    
}

/* ------------------------------------------------------------------------- */

void sys_message (int msgCode) {
	/*
	 * Prepackaged messages/exit statuses corresponding to an ID
	 */
	
}
