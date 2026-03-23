/*************************************************************************
 Authors:        Dan DeGenaro, Tian Li (Net IDs: drd92, tl995)
 Date:           Feb 27, 2026
 Last Updated:   Feb 27, 2026
 Purpose:        Exports functions from shell.c.
 Program:        shell.h
 Platform:       Linux, Solaris, BSD
 gcc Version:    gcc (GCC) 8.5.0 20210514 (Red Hat 8.5.0-28)
 Version:        1.0
*************************************************************************/

#ifndef SHELL_H
#define SHELL_H

//redirections
typedef struct {
    char *input_file;
    char *output_file;
    char *error_file;
    int append_output;
} Redirection;

void init_redirection(Redirection *redir);
char **strip_redirections(char **args, Redirection *redir);
int apply_redirections(const Redirection *redir);
void free_redirection(Redirection *redir);

// from shell.c
void print_prompt(void);
int read_line(char *buf, size_t buflen);
char **parse_line(char *line);
int is_builtin(const char *command);
int execute_builtin_command(char **args);
void execute_external_command(char **args);
void free_args(char **args);

#define MAX_LINE 256
#define MAX_ARGS 20
#define MAX_DIR_LEN 1024

#endif