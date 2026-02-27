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

// from shell.c
void print_prompt();
char *read_line();
char **parse_line();
int is_builtin();
int execute_builtin_command();
void execute_external();
void free_args();

#endif