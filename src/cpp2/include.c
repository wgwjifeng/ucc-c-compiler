#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

#include "../util/dynarray.h"
#include "../util/alloc.h"
#include "../util/util.h"

#include "include.h"

/* CPP_DIE() */
#include "main.h"
#include "preproc.h"

static struct include_ent
{
	char *dir;
	int issys;
} **include_dirs;

void include_add_dir(char *d, int system)
{
	struct include_ent *ent = umalloc(sizeof *ent);
	ent->dir = d;
	ent->issys = system;

	dynarray_add(&include_dirs, ent);
}

int include_is_sysheader(const char *d)
{
	fprintf(stderr, "\e[1;35mIN SYSH(%s): ", d);

	struct include_ent **i;
	for(i = include_dirs; i && *i; i++){
		struct include_ent *ent = *i;
		fprintf(stderr, "strcmp('%s', '%s')\n", d, ent->dir);
		if(ent->issys && !strcmp(d, ent->dir)){
			fprintf(stderr, "1\e[m\n");
			return 1;
		}
	}
			fprintf(stderr, "0\e[m\n");
	return 0;
}

FILE *include_fopen(const char *fnam)
{
	FILE *f;
	char *rslash = strrchr(fnam, '/');

	if(!(rslash ? rslash[1] : *fnam))
		CPP_DIE("empty filename");

	f = fopen(fnam, "r");
	if(f)
		return f;

	if(errno != ENOENT)
		CPP_DIE("open: %s:", fnam);

	return NULL;
}

FILE *include_search_fopen(const char *cd, const char *fnam, char **const ppath)
{
	FILE *f = NULL;
	int i;

	trace("include \"%s\", cd=%s\n", fnam, cd);

	*ppath = NULL;

retry:
	for(i = 0; include_dirs && include_dirs[i]; i++){
		char *path;

		if(cd){
			path = ustrprintf(
					"%s/%s/%s",
					*include_dirs[i]->dir == '/' ? "" : cd,
					include_dirs[i]->dir,
					fnam);
		}else{
			path = ustrprintf("%s/%s", include_dirs[i]->dir, fnam);
		}

		trace("  trying %s...\n", path);

		f = include_fopen(path);
		if(f){
			trace(" found @ %s\n", path);
			*ppath = path;
			break;
		}
		free(path);
	}

	if(!f && cd){
		cd = NULL;
		goto retry;
	}

	return f;
}
