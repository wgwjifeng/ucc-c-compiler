#include <stdio.h>
#include <stdlib.h>

#include <errno.h>

#include "io.h"
#include "alloc.h"

char *fline(FILE *f, int *const newline)
{
	int c, pos, len;
	char *line;

	if(newline)
		*newline = 0;

	if(feof(f) || ferror(f))
		return NULL;

	pos = 0;
	len = 10;
	line = umalloc(len);

	do{
		errno = 0;
		if((c = fgetc(f)) == EOF){
			if(errno == EINTR)
				continue;
			if(pos)
				return line;
			free(line);
			return NULL;
		}

		line[pos++] = c;
		if(pos == len){
			const size_t old = len;
			len *= 2;
			line = urealloc(line, len, old);
			line[pos] = '\0';
		}

		if(c == '\n'){
			if(newline)
				*newline = 1;
			line[pos-1] = '\0';
			return line;
		}
	}while(1);
}
