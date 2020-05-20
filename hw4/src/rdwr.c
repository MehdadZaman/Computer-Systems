/*
 * Utility functions for reading and writing integer values.
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "debug.h"

/*
 * Read from an input stream a block of data of a specified number of bytes.
 */
int read_data(FILE *in, char *resultp, int nbytes) {
    debug("[%d:      ] Read data (fd = %d, nbytes = %d)", getpid(), fileno(in), nbytes);
    for(int i = 0; i < nbytes; i++) {
	int c;
	if((c = fgetc(in)) == EOF)
	    return EOF;
	*resultp++ = c & 0xff;
	//debug("[%d] read 0x%x", getpid(), c);
    }
    return nbytes;
}

/*
 * Write to an output stream a block of data of a specified number of bytes.
 */
int write_data(FILE *out, char *datap, int nbytes) {
    debug("[%d:      ] Write data (fd = %d, nbytes = %d)", getpid(), fileno(out), nbytes);
    for(int i = 0; i < nbytes; i++) {
	//debug("[%d] write 0x%x", getpid(), *datap);
	if(fputc(*datap++, out) == EOF)
	    return EOF;
    }
    return nbytes;
}
