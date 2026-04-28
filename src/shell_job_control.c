/*************************************************************************
 Authors:        Tian Li, Dan DeGenaro (Net IDs: tl995, drd92)
 Date:           Apr 9, 2026
 Last Updated:   Apr 28, 2026
 Purpose:        Implements main functionalities for bash emulation program.
 Program:        shell.c
 Platform:       Linux, Solaris, BSD
 gcc Version:    gcc (GCC) 8.5.0 20210514 (Red Hat 8.5.0-28)
 Version:        4.0
*************************************************************************/

#include <ctype.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include "shell.h"

static JobTable global_job_table = { NULL, 1, 0 };
pid_t shell_pid = 0; // Shell's own PID
pid_t foreground_pid = 0; // PID of current foreground process group
pid_t current_child_pid = 0; // Used by SIGINT handler



/*
    Initializes the job table.
*/
void init_job_table(void) {
    shell_pid = getpid(); // PID of the shell process
    global_job_table.head = NULL; // head of list
    global_job_table.next_id = 1; // first ID will be 1
    global_job_table.total_jobs = 0; // no jobs at present
    // Put shell in its own process group
    setpgid(0, 0);

    if (isatty(STDIN_FILENO)) {
        tcsetpgrp(STDIN_FILENO, shell_pid);
    }
}



/*
    Looks for the background operator & at the end of the line,
        ignoring surrounding whitespace.

    Args:
        `char *line`: The line to be parsed. The & operator will be removed from
            the line along with surrounding whitespace.
    Returns:
        `int`: 1 if the & operator was found, else 0.
*/
int parse_background_operator(char *line) {
    size_t len = strlen(line);

    // ignore trailing whitespace
    while (len > 0 && isspace((unsigned char)line[len - 1])) {
        len--;
    }

    // found background operator at the end of the line
    if (len > 0 && line[len - 1] == '&') {
        line[len - 1] = '\0'; // replace with terminator

        // trim trailing whitespace
        len = strlen(line);
        while (len > 0 && isspace((unsigned char)line[len - 1])) {
            line[--len] = '\0';
        }

        return 1; // 1 because & was found
    }

    return 0; // 0 if no & found
}



/*
    Finds a job in the job table using its integer ID.

    Args:
        `int job_id`: The integer ID of the job to search for.
    Returns:
        `Job *`: A pointer to that job if it exists, else `NULL`.
*/
Job *find_job_by_id(int job_id) {

    // search list, return job if id matches
    Job *curr = global_job_table.head;
    while (curr != NULL) {
        if (curr->job_id == job_id) return curr;
        if (job_id <= 0 && curr->next == NULL) {
            return curr;
        }
        curr = curr->next;
    }
    return NULL; // not found
}



/*
    Finds a job in the job table using its process ID (PID).

    Args:
        `pid_t pid`: The process ID of the job to search for.
    Returns:
        `Job *`: A pointer to that job if it exists, else `NULL`.
*/
Job *find_job_by_pid(pid_t pid) {

    // search list, return job if id matches
    Job *curr = global_job_table.head;
    while (curr != NULL) {
        if (curr->pid == pid) return curr;
        curr = curr->next;
    }
    return NULL; // not found
}



/*
    Adds a job to the jobs table.

    Args:
        `pid_t pid`: The Process ID of the job.
        `pid_t pgid`: The Process Group ID of the job.
        `const char *cmd`: The command associated with this job.
        `int is_background`: Whether to run it in the background.
    Returns:
        `int`: The job ID assigned if successful, else -1.
*/
int add_job_phase4(pid_t pid, pid_t pgid, const char *cmd, int is_background) {
    Job *j = malloc(sizeof(Job)); // allocate memory for a job struct in the table
    if (j == NULL) { // handle OOM
        perror("malloc");
        return -1;
    }

    j->job_id = global_job_table.next_id++; // get next id and add 1 for future job
    j->pid = pid; // set job PID
    j->command_line = strdup(cmd); // command associated with this job
    if (j->command_line == NULL) { // issue with copying the cmd
        free(j);
        perror("strdup");
        return -1;
    }

    j->status = JOB_RUNNING; // set running
    j->next = NULL; // send to end of list
    if (global_job_table.head == NULL) { // empty list, make head
        global_job_table.head = j;
    } else { // traverse list to end and append
        Job *tail = global_job_table.head;
        while (tail->next) {
            tail = tail->next;
        }
        tail->next = j;
    }
    global_job_table.total_jobs++; // added 1 job

    (void)pgid; 

    if (is_background) { // print a message if the job has been put in the bg
        printf("[%d] %d\n", j->job_id, pid);
        fflush(stdout);
    }

    return j->job_id;
}



/*
    Convert a `JobStatus` struct into a string representation.

    Args:
        `JobStatus status`: The status to be converted.
    Returns:
        `char *`: Either "Running", "Stopped", "Done", or "Unknown".
*/
static const char *job_status_string(JobStatus status) {
    switch (status) {
        case JOB_RUNNING:
            return "Running";
        case JOB_STOPPED:
            return "Stopped";
        case JOB_DONE:
            return "Done";
        default:
            return "Unknown";
    }
}



/*
    Executes the `jobs` command as a built-in.
*/
void builtin_jobs(void) {

    // start from head of table's list
    Job *curr = global_job_table.head;

    // iterate over list
    while (curr != NULL) {
        if (curr->status != JOB_DONE) {
            // print any job that is not "Done"
            printf(
                "[%d] %s %s\n",
                curr->job_id,
                job_status_string(curr->status),
                curr->command_line
            );
        }
        curr = curr->next;
    }
}



/*
    Executes the `fg` command as a built-in.

    Args:
        `int job_id`: The job ID (not PID) to bring to the foreground.
*/
void builtin_fg(int job_id) {

    // find the job
    Job *job = find_job_by_id(job_id);
    if (job == NULL) { // not found
        printf("myshell: fg: job %d not found\n", job_id);
        return;
    }

    // display command associated with the job
    printf("%s\n", job->command_line);
    fflush(stdout);

    // pid of job becomes pgid
    pid_t pgid = job->pid;

    // set stdin to this process
    if (tcsetpgrp(STDIN_FILENO, pgid) < 0) {
        perror("tcsetpgrp");
        return;
    }

    // set fg process to this process
    foreground_pid = pgid;
    current_child_pid = job->pid;

    // 
    if (job->status == JOB_STOPPED) {
        kill(-pgid, SIGCONT);
    }
    job->status = JOB_RUNNING;

    // wait for the new fg job to complete
    int status;
    if (waitpid(job->pid, &status, WUNTRACED) < 0) {
        perror("waitpid");
    }

    // set stdin back to the shell
    tcsetpgrp(STDIN_FILENO, shell_pid);
    foreground_pid = 0; // put shell back into fg
    current_child_pid = 0;
    
    // if job exited or sent a signal, set to done, or to stopped if "stopped"
    if (WIFEXITED(status) || WIFSIGNALED(status)) {
        job->status = JOB_DONE;
    } else if (WIFSTOPPED(status)) {
        job->status = JOB_STOPPED;
        printf("\n[%d] Stopped %s\n", job->job_id, job->command_line);
        fflush(stdout);
    }
}



/*
    Executes the `bg` command as a built-in.

    Args:
        `int job_id`: The job ID (not PID) to bring to the foreground.
*/
void builtin_bg(int job_id) {

    // search for job
    Job *job = find_job_by_id(job_id);

    // not found
    if (job == NULL) {
        printf("myshell: bg: job not found\n");
        return;
    }

    // already done
    if (job->status == JOB_DONE) {
        printf("myshell: bg: job already finished\n");
        return;
    }

    // submit continue signal to the job
    kill(-job->pid, SIGCONT);
    job->status = JOB_RUNNING;
    printf("[%d] %d\n", job->job_id, job->pid);
    fflush(stdout);
}



/*
    Clean up any done jobs by removing them from the table and printing
        notifications.
*/
void cleanup_done_jobs(void) {

    // iterate table
    Job *curr = global_job_table.head;
    Job *prev = NULL;
    while (curr != NULL) {
        if (curr->status == JOB_DONE) { // print done jobs
            printf("\n[%d] Done %s\n", curr->job_id, curr->command_line);
            fflush(stdout);

            Job *tmp = curr; // remove from list
            if (prev == NULL) {
                global_job_table.head = curr->next;
            } else {
                prev->next = curr->next;
            }

            curr = curr->next;
            free(tmp->command_line);
            free(tmp);
            global_job_table.total_jobs--;
        } else { // if not a done job, just move to the next one
            prev = curr;
            curr = curr->next;
        }
    }
}



/*
    Free up the entire job table.
*/
void free_job_table(void) {
    Job *curr = global_job_table.head;
    while (curr != NULL) {
        Job *tmp = curr;
        curr = curr->next;
        free(tmp->command_line);
        free(tmp);
    }

    global_job_table.head = NULL;
    global_job_table.total_jobs = 0;
}



/*
    Handles child signal.

    Args:
        `int sig`: The signal.
*/
void handle_sigchld(int sig) {
    (void)sig;

    pid_t pid;
    int status;

    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {
        Job *job = find_job_by_pid(pid);
        if (job == NULL) continue;

        if (WIFEXITED(status) || WIFSIGNALED(status)) {
            job->status = JOB_DONE;
        } else if (WIFSTOPPED(status)) {
            job->status = JOB_STOPPED;
        }
    }
}



/*
    Handles interrupt signal.

    Args:
        `int sig`: The signal.
*/
void handle_sigint(int sig) {
    (void)sig;
    if (foreground_pid != 0) { // forward to fg process
        kill(-foreground_pid, SIGINT);
    }
}



/*
    Handles stop signal.

    Args:
        `int sig`: The signal.
*/
void handle_sigtstp(int sig) {
    (void)sig;
    if (foreground_pid != 0) { // forward to fg process
        kill(-foreground_pid, SIGTSTP);
    }
}



/*
    Sets up handlers for all important signals that could be sent via the terminal.
*/
void setup_signal_handlers(void) {
    struct sigaction sa;

    // SIGTTOU/SITTTIN - ignore so shell can call tcsetpgrp without being stopped
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);

    // SIGCHILD - auto-reap background children
    sa.sa_handler = handle_sigchld;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, NULL);

    // SIGINT - forward Ctrl+C to foreground child
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    // SIGTSTP - forward Ctrl+Z to foreground child
    sa.sa_handler = handle_sigtstp;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGTSTP, &sa, NULL);

    // SIGPIPE - ignore broken pipes
    signal(SIGPIPE, SIG_IGN);
}



/*
    Restores all the signal handlers that were overwritten.
*/
void restore_signal_handlers(void) {
    signal(SIGCHLD, SIG_DFL);
    signal(SIGINT, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);
    signal(SIGPIPE, SIG_DFL);
}
