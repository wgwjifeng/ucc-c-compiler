#ifndef INCLUDE_H
#define INCLUDE_H

void include_add_dir(char *, int is_sys);
int include_is_sysheader(const char *);
FILE *include_search_fopen(const char *cd, const char *fnam, char **ppath);
FILE *include_fopen(const char *fnam);

#endif
