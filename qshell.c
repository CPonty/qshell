//
//  qshell.c
//

#include "qshell.h"

/* ------------------------------------------------------------------------- */

int main (int argc, char * argv[]) {
	init();
	parse_args(argc, argv);	
	return 0;
}

void init () {
	/*
	 * Set initial values for global variables
	 */
	input = stdin;
}

void stop () {
	/*
	 * About to close shell
	 *  - stop background processes
	 *  - free memory
	 *  - exit with status 0 (OK)
	 */
	#if DEBUG>0
	fprintf(stderr, "Exiting, status 0\n");
	#else
	fprintf(stderr, "\n");
	#endif
	fflush(stderr);
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

void read_input () {
	/*
	 * Main event loop to collect commands from input lines
	 */
	char c, strbuf[256], *bufp;
	char *argv[20], **argp;
	int charc, argc;

	while (1) {
		// 1. Print prompt
		if (getcwd(strbuf,sizeof(strbuf))==NULL) {
			fprintf(stderr, "getcwd() error : %s\n", 
				strerror(errno));
			exit(2);
		}
		fprintf(stdout, "[%s]? ",strbuf);
		fflush(stdout);

		// 2. Read from input stream. Max 128 characters.
		//    Split on newline/EoF
		charc=0;
		c='\0';
		while ((c=fgetc(input))!='\n' && !feof(input) && (charc<128)) {
			strbuf[charc++]=c;
		}
		strbuf[charc]='\0';
		//Line too long: flush characters until the next newline
		if (charc==128) {
			while (fgetc(input)!='\n' && !feof(input));
		}
		//File input: print the command line next to the prompt
		if (input!=stdin) {
			fprintf(stdout, "%s\n", strbuf);
			fflush(stdout);
		}
		if (charc==128) {
			fprintf(stderr, WARN_CMDLINE_CHARS);
		}

		// 3. Separate arguments. Max 20 arguments.
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
		if (argc>20) {
			argc=20;
			fprintf(stderr, WARN_CMDLINE_ARGS);
		}

		// 4. Process input
		//     - Ignore blank or comment lines
		if (argc>0) {
			if (argv[0][0]!='#') {
				parse_input(argc, argv);
			}
		}

		// 5. Print background process output/termination
		//
		//
		//

		// 6. STOP before we prompt again - is it EoF?
		//     - For interactive mode, this is any zero-char read
		//     - For file mode, we want to stop anytime we hit feof
		if (((charc==0) && (c!='\n')) || \
		    (input!=stdin && feof(input))) {
			stop();
		}

	}
}

void parse_input (int argc, char * argv[]) {
	/*
	 * Handle a list of arguments from an input line
	 */
	#if DEBUG>1
	fprintf(stderr, "argc=%d\n", argc);
	for (int i=0; i<argc; i++) {
		fprintf(stderr, "arg[%d]=%s\n", i, argv[i]);
	}
	#endif
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


void sys_message (int msgCode) {
	/*
	 * Prepackaged messages/exit statuses corresponding to an ID
	 */
	
}
