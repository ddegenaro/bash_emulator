#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <termios.h>
#include <ctype.h>

#include "shell.h"

static JobTable global_job_table = { NULL, 1, 0 };
pid_t shell_pid = 0; // Shell's own PID
pid_t foreground_pid = 0; // PID of current foreground process group
pid_t current_child_pid = 0; // Used by SIGINT handler

void init_job_table(void) {
    shell_pid = getpid();
    global_job_table.head = NULL;
    global_job_table.next_id = 1;
    global_job_table.total_jobs = 0;
    // Put shell in its own process group
    setpgid(0, 0);
}

int parse_background_operator(char *line) {
    size_t len = strlen(line);

    while (len > 0 && isspace((unsigned char)line[len - 1])) {
        len--;
    }

    if (len > 0 && line[len - 1] == '&') {
        line[len - 1] = '\0';

        len = strlen(line);
        while (len > 0 && isspace((unsigned char)line[len - 1])) {
            line[--len] = '\0';
        }

        return 1;
    }

    return 0;
}

Job *find_job_by_id(int job_id) {
    Job *curr = global_job_table.head;
    while (curr != NULL) {
        if (curr->job_id == job_id) return curr;
        curr = curr->next;
    }
    return NULL;
}

Job *find_job_by_pid(pid_t pid) {
    Job *curr = global_job_table.head;
    while (curr != NULL) {
        if (curr->pid == pid) return curr;
        curr = curr->next;
    }
    return NULL;
}

int add_job_phase4(pid_t pid, pid_t pgid, const char *cmd, int is_background) {
    Job *j = malloc(sizeof(Job)); // allocate memory for a job in the table
    if (j == NULL) { // handle OOM
        perror("malloc");
        return -1;
    }

    j->job_id = global_job_table.next_id++; // get next id and add 1 for future job
    j->pid = pid; // set job PID
    j->command_line = strdup(cmd); 
    if (j->command_line == NULL) {
        free(j);
        perror("strdup");
        return -1;
    }

    j->status = JOB_RUNNING;
    j->next = global_job_table.head;
    global_job_table.head = j;
    global_job_table.total_jobs++;

    (void)pgid;

    if (is_background) {
        printf("[%d] %d\n", j->job_id, pid);
        fflush(stdout);
    }

    return j->job_id;
}

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

void builtin_jobs(void) {
    Job *curr = global_job_table.head;
    while (curr != NULL) {
        if (curr->status != JOB_DONE) {
            printf("[%d] %s %s\n",
                   curr->job_id,
                   job_status_string(curr->status),
                   curr->command_line);
        }
        curr = curr->next;
    }
}

void builtin_fg(int job_id) {
    Job *job = find_job_by_id(job_id);
    if (job == NULL) {
        printf("myshell: fg: job not found\n");
        return;
    }

    printf("%s\n", job->command_line);
    fflush(stdout);

    pid_t pgid = job->pid;

    if (tcsetpgrp(STDIN_FILENO, pgid) < 0) {
        perror("tcsetpgrp");
        return;
    }

    foreground_pid = pgid;
    current_child_pid = job->pid;

    if (job->status == JOB_STOPPED) {
        kill(-pgid, SIGCONT);
    }
    job->status = JOB_RUNNING;

    int status;
    if (waitpid(job->pid, &status, WUNTRACED) < 0) {
        perror("waitpid");
    }

    tcsetpgrp(STDIN_FILENO, shell_pid);
    foreground_pid = 0;
    current_child_pid = 0;

    if (WIFEXITED(status) || WIFSIGNALED(status)) {
        job->status = JOB_DONE;
    } else if (WIFSTOPPED(status)) {
        job->status = JOB_STOPPED;
        printf("\n[%d] Stopped %s\n", job->job_id, job->command_line);
        fflush(stdout);
    }
}

void builtin_bg(int job_id) {
    Job *job = find_job_by_id(job_id);
    if (job == NULL) {
        printf("myshell: bg: job not found\n");
        return;
    }

    if (job->status == JOB_DONE) {
        printf("myshell: bg: job already finished\n");
        return;
    }

    kill(-job->pid, SIGCONT);
    job->status = JOB_RUNNING;
    printf("[%d] %d\n", job->job_id, job->pid);
    fflush(stdout);
}

void cleanup_done_jobs(void) {
    Job *curr = global_job_table.head;
    Job *prev = NULL;

    while (curr != NULL) {
        if (curr->status == JOB_DONE) {
            Job *tmp = curr;

            if (prev == NULL) {
                global_job_table.head = curr->next;
            } else {
                prev->next = curr->next;
            }

            curr = curr->next;
            free(tmp->command_line);
            free(tmp);
            global_job_table.total_jobs--;
        } else {
            prev = curr;
            curr = curr->next;
        }
    }
}

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

void handle_sigchld(int sig) {
    (void)sig;

    pid_t pid;
    int status;

    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {
        Job *job = find_job_by_pid(pid);
        if (job == NULL) continue;

        if (WIFEXITED(status) || WIFSIGNALED(status)) {
            job->status = JOB_DONE;
            if (pid != foreground_pid) {
                printf("\n[%d] Done %s\n", job->job_id, job->command_line);
                fflush(stdout);
            }
        } else if (WIFSTOPPED(status)) {
            job->status = JOB_STOPPED;
            if (pid != foreground_pid) {
                printf("\n[%d] Stopped %s\n", job->job_id, job->command_line);
                fflush(stdout);
            }
        }
    }
}

void handle_sigint(int sig) {
    (void)sig;
    if (foreground_pid != 0) {
        kill(-foreground_pid, SIGINT);
    }
}

void handle_sigtstp(int sig) {
    (void)sig;
    if (foreground_pid != 0) {
        kill(-foreground_pid, SIGTSTP);
    }
}

void setup_signal_handlers(void) {
    struct sigaction sa;

    sa.sa_handler = handle_sigchld;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, NULL);

    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    sa.sa_handler = handle_sigtstp;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGTSTP, &sa, NULL);

    signal(SIGPIPE, SIG_IGN);
}

void restore_signal_handlers(void) {
    signal(SIGCHLD, SIG_DFL);
    signal(SIGINT, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);
    signal(SIGPIPE, SIG_DFL);
}