/*************************************************************************
 Authors:        Dan DeGenaro, Tian Li (Net IDs: drd92, tl995)
 Date:           Feb 27, 2026
 Last Updated:   Apr 27, 2026
 Purpose:        Runs REPL loop for bash emulation program.
 Program:        main.c
 Platform:       Linux, Solaris, BSD
 gcc Version:    gcc (GCC) 8.5.0 20210514 (Red Hat 8.5.0-28)
 Version:        4.0
*************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

#include "shell.h"



/*
    Entry point to the program. Handles REPL loop.

    Returns:
        `int`: 0 on successful exit.
*/
int main(void) {
    int exit_status = 1;
    char line[MAX_LINE]; // buffer to be read from terminal

    init_job_table(); // create a job table for bg processes
    setup_signal_handlers(); // 

    printf("\nmyshell 4.0\n\n");

    while (exit_status) {
        cleanup_done_jobs();

        print_prompt();

        if (!read_line(line, sizeof(line))) break;

        // continuation prompt for unclosed quotes
        while (1) {
            char in_quote = 0;
            for (char *p = line; *p; ++p) {
                if ((*p == '\'' || *p == '"') && !in_quote) in_quote = *p;
                else if (*p == in_quote) in_quote = 0;
            }
            if (!in_quote) break;

            printf("> ");
            fflush(stdout);

            size_t len = strlen(line);
            if (len + 1 >= sizeof(line)) break;
            line[len] = '\n';
            if (!read_line(line + len + 1, sizeof(line) - len - 1)) break;
        }

        if (line[0] == '\0') continue;

        int is_background = parse_background_operator(line);

        PipeOperator op_type;
        if (find_next_op(line, &op_type) != NULL) {
            Pipeline *p = parse_pipeline(line);
            if (p != NULL) {
                if (!is_background) {
                    execute_pipeline(p);
                } else {
                    pid_t pid = fork();
                    if (pid == 0) {
                        setpgid(0, 0);
                        execute_pipeline(p);
                        free_pipeline(p);
                        exit(EXIT_SUCCESS);
                    } else if (pid > 0) {
                        setpgid(pid, pid);
                        add_job_phase4(pid, pid, line, 1);
                    } else {
                        perror("fork");
                    }
                }
                free_pipeline(p);
            }
            continue;
        }

        char line_copy[MAX_LINE];
        strncpy(line_copy, line, MAX_LINE - 1);
        line_copy[MAX_LINE - 1] = '\0';

        char **args = parse_line(line_copy);
        if (args[0] == NULL) {
            free_args(args);
            continue;
        }

        if (is_builtin(args[0])) {
            exit_status = execute_builtin_command(args);
        } else {
            if (!is_background) {
                execute_external_command(args);
            } else {
                pid_t pid = fork();
                if (pid == 0) {
                    setpgid(0, 0);

                    Redirection redir;
                    init_redirection(&redir);

                    char **clean_args = strip_redirections(args, &redir);
                    if (clean_args == NULL || clean_args[0] == NULL) {
                        free_redirection(&redir);
                        free_args(clean_args);
                        _exit(1);
                    }

                    char **expanded_args = expand_globs(clean_args);
                    free_args(clean_args);

                    if (apply_redirections(&redir) < 0) {
                        free_redirection(&redir);
                        free_args(expanded_args);
                        _exit(1);
                    }

                    execvp(expanded_args[0], expanded_args);
                    fprintf(stderr, "myshell: %s: command not found\n", expanded_args[0]);

                    free_redirection(&redir);
                    free_args(expanded_args);
                    _exit(127);
                } else if (pid > 0) {
                    setpgid(pid, pid);
                    add_job_phase4(pid, pid, line, 1);
                } else {
                    perror("fork");
                }
            }
        }

        free_args(args);
    }

    restore_signal_handlers();
    free_job_table();
    return 0;
}