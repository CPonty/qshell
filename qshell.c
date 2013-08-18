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

void parse_args(int argc, char * argv[]) {
	/*
	 * Decide on action to take at startup
	 */
	if (argc==1) {
		/* No parameters - start interactive input */
		
	} else if (argc==2) {
		/* One parameter - open as file to read stdin from */
		load_stdin(argv[1]);
	} else {
		/* 2+ paramters - not supported */
		printf(USAGE);
	}
}

void load_stdin (char * fname) {

}


void sys_message (int msgCode) {

}
