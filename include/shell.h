/*************************************************************************
 Authors:        Dan DeGenaro, Tian Li (Net IDs: drd92, tl995)
 Date:           Feb 27, 2026
 Last Updated:   Mar 22, 2026
 Purpose:        Exports functions from shell.c.
 Program:        shell.h
 Platform:       Linux, Solaris, BSD
 gcc Version:    gcc (GCC) 8.5.0 20210514 (Red Hat 8.5.0-28)
 Version:        1.0
*************************************************************************/

#ifndef SHELL_H
#define SHELL_H

#include <sys/types.h>
#include <stddef.h>

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

//glob functions
char **expand_globs(char **args);

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


//pipeline struct and operator enum
typedef enum {
    PIPE_NONE, // No pipe
    PIPE_BASIC, // | (pipe stdout to next command's stdin)
    PIPE_AND, // && (conditional execution - success)
    PIPE_OR, // || (conditional execution - failure)
    PIPE_SEQ // ; (sequential execution)
} PipeOperator;

typedef struct {
    char **commands; // Array of command strings
    PipeOperator *operators; // Operators between commands
    int num_commands; // Number of commands
} Pipeline;

const char *find_pipe_quoted(const char *str);
Pipeline *parse_pipeline(char *line);
void free_pipeline(Pipeline *p);
int execute_pipeline(Pipeline *p);


//job control
typedef enum {
    JOB_RUNNING,
    JOB_STOPPED,
    JOB_DONE
} JobStatus;

typedef struct Job {
    int job_id;
    pid_t pid;
    char *command_line;
    JobStatus status;
    struct Job *next;
} Job;

typedef struct {
    Job *head;
    int next_id;
    int total_jobs;
} JobTable;

void init_job_table(void);
void free_job_table(void);
void cleanup_done_jobs(void);

int add_job_phase4(pid_t pid, pid_t pgid, const char *cmd, int is_background);
Job *find_job_by_id(int job_id);
Job *find_job_by_pid(pid_t pid);

void builtin_jobs(void);
void builtin_fg(int job_id);
void builtin_bg(int job_id);

int parse_background_operator(char *line);

void setup_signal_handlers(void);
void restore_signal_handlers(void);

void handle_sigchld(int sig);
void handle_sigint(int sig);
void handle_sigtstp(int sig);

extern pid_t shell_pid;
extern pid_t foreground_pid;
extern pid_t current_child_pid;

#endif