#ifndef UCC_EXT_H
#define UCC_EXT_H

extern char **include_paths;
extern int time_subcmds;

char *actual_path(const char *prefix, const char *path);
void rename_or_move(char *old, char *new);
void cat(char *in, const char *out, int append);

char *ucc_where(void);

void execute(char *path, char **args);

int preproc(char *in, const char *out, char **args, int return_ec);
int compile(char *in, const char *out, char **args, int return_ec);
void assemble(char *in,    const char *out, char **args, char *as);
void link_all(char **objs, const char *out, char **args, char *ld);

void ucc_ext_cmds_show(int);
void ucc_ext_cmds_noop(int);

#endif
