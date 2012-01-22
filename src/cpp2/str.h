#ifndef STR_H
#define STR_H

char *word_replace(char *line, char *pos, const char *find, const char *replace);
char *word_find(   char *line, char *word);
char *str_replace(char *line, char *start, char *end, const char *replace);
int   word_replace_g(char **pline, char *find, const char *replace);
char *nest_close_paren(char *start);

#endif
