/*************************************************************************
 Authors:        Dan DeGenaro, Tian Li (Net IDs: drd92, tl995)
 Date:           Apr 9, 2026
 Last Updated:   Apr 9, 2026
 Purpose:        Implements pipe functionalities.
 Program:        shell_pipe.c
 Platform:       Linux, Solaris, BSD
 gcc Version:    gcc (GCC) 8.5.0 20210514 (Red Hat 8.5.0-28)
 Version:        1.0
*************************************************************************/
#include <string.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>

#include "shell.h"

/* Execute a pipeline of commands */
int execute_pipeline(Pipeline *p) {
    if (!p || p->num_commands <= 0) return -1;

    int num_pipes = p->num_commands - 1;
    int pipes[MAX_ARGS][2];
    pid_t pids[MAX_ARGS];

    for (int i = 0; i < num_pipes; i++) {
        if (pipe(pipes[i]) < 0) {
            perror("pipe");
            return -1;
        }
    }

    for (int i = 0; i < p->num_commands; i++) {
        pid_t pid = fork();

        if (pid < 0) {
            perror("fork");
            return -1;
        }

        if (pid == 0) {
            // stdin from previous pipe
            if (i > 0) {
                dup2(pipes[i - 1][0], STDIN_FILENO);
            }

            // stdout to next pipe
            if (i < p->num_commands - 1) {
                dup2(pipes[i][1], STDOUT_FILENO);
            }

            // close all pipe fds in child
            for (int j = 0; j < num_pipes; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }

            // reuse your existing phase-2 command parsing
            char line_copy[MAX_LINE];
            strncpy(line_copy, p->commands[i], MAX_LINE - 1);
            line_copy[MAX_LINE - 1] = '\0';

            char **args = parse_line(line_copy);
            if (args == NULL || args[0] == NULL) {
                free_args(args);
                _exit(1);
            }

            Redirection redir;
            init_redirection(&redir);

            char **expanded_args = expand_globs(args);
            char **clean_args = strip_redirections(expanded_args, &redir);

            if (apply_redirections(&redir) < 0) {
                free_redirection(&redir);
                free_args(args);
                free_args(expanded_args);
                free_args(clean_args);
                _exit(1);
            }

            execvp(clean_args[0], clean_args);
            fprintf(stderr, "myshell: %s: command not found\n", clean_args[0]);

            free_redirection(&redir);
            free_args(args);
            free_args(expanded_args);
            free_args(clean_args);
            _exit(127);
        }

        pids[i] = pid;
    }

    // parent closes all pipe fds
    for (int i = 0; i < num_pipes; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    int status = 0;
    int last_status = 0;

    for (int i = 0; i < p->num_commands; i++) {
        waitpid(pids[i], &status, 0);
        if (i == p->num_commands - 1) {
            last_status = status;
        }
    }

    if (WIFEXITED(last_status)) {
        return WEXITSTATUS(last_status);
    }

    return -1;
}

static char *trim_whitespace(char *s) {
    while (isspace((unsigned char)*s)) s++;
    if (*s == '\0') return s;

    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) {
        *end = '\0';
        end--;
    }
    return s;
}

Pipeline *parse_pipeline(char *line) {
    Pipeline *p = malloc(sizeof(Pipeline));
    if (!p) return NULL;

    p->commands = malloc(MAX_ARGS * sizeof(char *));
    p->operators = malloc(MAX_ARGS * sizeof(PipeOperator));
    p->num_commands = 0;

    if (!p->commands || !p->operators) {
        free(p->commands);
        free(p->operators);
        free(p);
        return NULL;
    }

    char *start = line;
    const char *op;

    while ((op = find_pipe_quoted(start)) != NULL) {
        size_t len = (size_t)(op - start);
        char *piece = malloc(len + 1);
        if (!piece) {
            free_pipeline(p);
            return NULL;
        }

        strncpy(piece, start, len);
        piece[len] = '\0';

        char *trimmed = trim_whitespace(piece);
        p->commands[p->num_commands] = strdup(trimmed);
        free(piece);

        p->operators[p->num_commands] = PIPE_BASIC;
        p->num_commands++;

        start = (char *)op + 1;
    }

    char *last = trim_whitespace(start);
    p->commands[p->num_commands] = strdup(last);
    p->num_commands++;

    return p;
}

const char *find_pipe_quoted(const char *str) {
    int in_single = 0;
    int in_double = 0;

    for (const char *p = str; *p != '\0'; ++p) {
        if (*p == '\'' && !in_double) {
            in_single = !in_single;
        } else if (*p == '"' && !in_single) {
            in_double = !in_double;
        } else if (*p == '|' && !in_single && !in_double) {
            // ignore ||
            if (*(p + 1) != '|') {
                return p;
            }
        }
    }

    return NULL;
}

void free_pipeline(Pipeline *p) {
    if (!p) return;

    if (p->commands) {
        for (int i = 0; i < p->num_commands; i++) {
            free(p->commands[i]);
        }
        free(p->commands);
    }

    free(p->operators);
    free(p);
}