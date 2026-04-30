/*************************************************************************
 Authors:        Tian Li, Dan DeGenaro (Net IDs: tl995, drd92)
 Date:           Apr 9, 2026
 Last Updated:   Apr 27, 2026
 Purpose:        Implements pipe functionalities.
 Program:        shell_pipe.c
 Platform:       Linux, Solaris, BSD
 gcc Version:    gcc (GCC) 8.5.0 20210514 (Red Hat 8.5.0-28)
 Version:        4.0
*************************************************************************/
#include <string.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>

#include "shell.h"



/*
    Removes whitespace at the end in place.

    Args:
        `char *s`: The string to trim.

    Returns:
        `static char *`: The same string, trimmed.
*/
static char *trim_whitespace(char *s) {
    while (isspace((unsigned char)*s)) s++; // move to first non-space
    if (*s == '\0') return s; // if all whitespace, return just the '\0'

    char *end = s + strlen(s) - 1; // move from end now
    while (end > s && isspace((unsigned char)*end)) {
        *end = '\0'; // stick '\0' after last non-whitespace
        end--;
    }
    return s;
}



/*
    Execute a pipeline of commands.

    Args:
        `Pipeline *p`: The pipeline to execute.
    Returns:
        `int`: -1 if error or empty pipeline.
*/
int execute_pipeline(Pipeline *p , int is_background) {
    if (!p || p->num_commands <= 0) return -1;

    int last_status = 0;
    int i = 0;

    while (i < p->num_commands) {

        // find the end of the current PIPE_BASIC segment
        int seg_start = i;
        int seg_end = i;
        while (seg_end < p->num_commands - 1 && p->operators[seg_end] == PIPE_BASIC) {
            seg_end++;
        }

        // check short-circuit from previous operator
        if (i > 0) {
            PipeOperator prev_op = p->operators[seg_start - 1];
            if (prev_op == PIPE_AND && last_status != 0) { i = seg_end + 1; continue; }
            if (prev_op == PIPE_OR  && last_status == 0) { i = seg_end + 1; continue; }
        }

        // execute seg_start..seg_end as a true pipe
        int seg_len = seg_end - seg_start + 1;

        // if single command, check for builtin first
        if (seg_len == 1) {
            char line_copy[MAX_LINE];
            strncpy(line_copy, p->commands[seg_start], MAX_LINE - 1);
            line_copy[MAX_LINE - 1] = '\0';
            
            char **args = parse_line(line_copy);

            if (args && args[0] && is_builtin(args[0])) {
                int ret = execute_builtin_command(args);
                last_status = (ret == 0) ? 1 : 0; // invert: 0=exit means failure, 1=continue means success
                free_args(args);
                i = seg_end + 1;
                continue;
            }
            free_args(args);
        }

        // need a link between each segment
        int num_pipes = seg_len - 1;
        int pipes[MAX_ARGS][2];
        pid_t pids[MAX_ARGS];
        pid_t pgid = 0;

        // allocate pipe between each adjacent pair of segments
        for (int j = 0; j < num_pipes; j++) {
            if (pipe(pipes[j]) < 0) { perror("pipe"); return -1; }
        }

        // create forks for each segment
        for (int j = 0; j < seg_len; j++) {
            pid_t pid = fork();
            if (pid < 0) { perror("fork"); return -1; }

            // if we're the child process
            if (pid == 0) { // need to restore everything but sigchld
                // signal(SIGINT, SIG_DFL);
                // signal(SIGTSTP, SIG_DFL);
                signal(SIGPIPE, SIG_DFL);
                setpgid(0, pgid);

                // redirect piped output and input
                if (j > 0)            dup2(pipes[j-1][0], STDIN_FILENO);
                if (j < seg_len - 1)  dup2(pipes[j][1],   STDOUT_FILENO);

                // close pipes now they are used
                for (int k = 0; k < num_pipes; k++) {
                    close(pipes[k][0]);
                    close(pipes[k][1]);
                }

                // copy next command
                char line_copy[MAX_LINE];
                strncpy(line_copy, p->commands[seg_start + j], MAX_LINE - 1);
                line_copy[MAX_LINE - 1] = '\0';

                // parse it
                char **args = parse_line(line_copy);
                if (args == NULL || args[0] == NULL) { free_args(args); _exit(1); }

                // init redirections
                Redirection redir;
                init_redirection(&redir);

                // globs and redirect parsing
                char **expanded_args = expand_globs(args);
                char **clean_args = strip_redirections(expanded_args, &redir);

                if (apply_redirections(&redir) < 0) {
                    free_redirection(&redir);
                    free_args(args);
                    free_args(expanded_args);
                    free_args(clean_args);
                    _exit(1);
                }

                // execute command
                execvp(clean_args[0], clean_args);
                fprintf(stderr, "myshell: %s: command not found\n", clean_args[0]);

                // free
                free_redirection(&redir);
                free_args(args);
                free_args(expanded_args);
                free_args(clean_args);
                _exit(127);
            }

            if (pgid == 0) pgid = pid;
            setpgid(pid, pgid);
            pids[j] = pid;
        }

        for (int j = 0; j < num_pipes; j++) {
            close(pipes[j][0]);
            close(pipes[j][1]);
        }
        
        
        if (is_background) {
            // Background job:
            // Do NOT take control of the terminal and do NOT wait.

            // Store this pipeline as a job using its PGID (first child PID)
            // Both arguments are pgid because the whole pipeline shares one process group
            add_job_phase4(pgid, pgid, p->commands[seg_start], 1);

            // For background jobs, we return immediately to the prompt
            // so just mark last_status as success
            last_status = 0;

        } else {
            // Foreground job:
            // This pipeline should take control of the terminal and block the shell

            // Track which process group is currently in the foreground
            // so signal handlers (Ctrl+C, Ctrl+Z) know where to send signals
            foreground_pid = pgid;

            // Optionally track first child PID (used in some implementations)
            current_child_pid = pids[0];

            // Give terminal control to the pipeline's process group
            // so keyboard signals go to the pipeline instead of the shell
            if (isatty(STDIN_FILENO))
                tcsetpgrp(STDIN_FILENO, pgid);

            int status = 0;

            // Wait for all processes in the pipeline to finish or stop
            // WUNTRACED allows us to detect Ctrl+Z (stopped processes)
            for (int j = 0; j < seg_len; j++) {
                waitpid(pids[j], &status, WUNTRACED);
            }

            // Save exit status (typically from the last command)
            last_status = WIFEXITED(status) ? WEXITSTATUS(status) : -1;

            // After the pipeline finishes, give terminal control back to the shell
            if (isatty(STDIN_FILENO))
                tcsetpgrp(STDIN_FILENO, shell_pid);

            // Clear foreground tracking since no job is running now
            foreground_pid = 0;
            current_child_pid = 0;
        }

        i = seg_end + 1;
    }

    return last_status;
}


const char *find_next_op(const char *str, PipeOperator *op_type) {
    int in_single = 0, in_double = 0;

    for (const char *p = str; *p; ++p) {
        if (*p == '\'' && !in_double) in_single = !in_single;
        else if (*p == '"' && !in_single) in_double = !in_double;
        else if (!in_single && !in_double) {
            if (p[0] == '&' && p[1] == '&') { *op_type = PIPE_AND;   return p; }
            if (p[0] == '|' && p[1] == '|') { *op_type = PIPE_OR;    return p; }
            if (p[0] == '|')                { *op_type = PIPE_BASIC; return p; }
            if (p[0] == ';')                { *op_type = PIPE_SEQ;   return p; }
        }
    }
    return NULL;
}



/*
    Parses a pipeline execution plan from the entered line.

    Args:
        `char *line`: The input string.
    Returns:
        `Pipeline`: A struct containing the components of the pipeline to execute.
*/
Pipeline *parse_pipeline(char *line) {

    // try to allocate a pipeline
    Pipeline *p = malloc(sizeof(Pipeline));
    if (!p) return NULL;

    // allocate space
    p->commands = malloc(MAX_ARGS * sizeof(char *));
    p->operators = malloc(MAX_ARGS * sizeof(PipeOperator));
    p->num_commands = 0;

    // memory failure, give up
    if (!p->commands || !p->operators) {
        free(p->commands);
        free(p->operators);
        free(p);
        return NULL;
    }

    char *start = line;

    // scan for pipe operators
    while (*start) {

        PipeOperator op_type;
        const char *op = find_next_op(start, &op_type);
        
        if (op) {
            size_t len = op - start;
            char *piece = malloc(len + 1);
            if (!piece) {
                free_pipeline(p);
                return NULL;
            }

            strncpy(piece, start, len);
            piece[len] = '\0';

            p->commands[p->num_commands] = strdup(trim_whitespace(piece));
            free(piece);

            p->operators[p->num_commands] = op_type;
            p->num_commands++;

            int op_len;
            switch (op_type) {
                case PIPE_BASIC: op_len = 1; break;
                case PIPE_AND: op_len = 2; break;
                case PIPE_OR: op_len = 2; break;
                case PIPE_SEQ: op_len = 1; break;
                default: op_len = 1; break;
            }
            start = (char *) op + op_len;
        
        } else {
            p->commands[p->num_commands] = strdup(trim_whitespace(start));
            p->num_commands++;
            break;
        }
    }

    return p;
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