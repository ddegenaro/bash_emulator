/*************************************************************************
 Authors:        Dan DeGenaro, Tian Li (Net IDs: drd92, tl995)
 Date:           Feb 27, 2026
 Last Updated:   Apr 25, 2026
 Purpose:        Implements main functionalities for bash emulation program.
 Program:        shell.c
 Platform:       Linux, Solaris, BSD
 gcc Version:    gcc (GCC) 8.5.0 20210514 (Red Hat 8.5.0-28)
 Version:        1.0
*************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <glob.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>

#include "shell.h"

const char *builtins[] = {"exit", "cd", "pwd", "jobs", "fg", "bg", NULL};

/*
    Prints the bash prompt.
*/
void print_prompt() {
    if (isatty(fileno(stdin))) {
        printf("myshell--> ");
        fflush(stdout);
    }
}



/*
    Reads a line of input sent from the bash terminal.
    Args:
        `char *buf`: A buffer to store the line of input.
        `size_t buflen`: The length of the buffer.
*/
// int read_line(char *buf, size_t buflen) {
//     if (fgets(buf, buflen, stdin) == NULL) return 0; // EOF
//     size_t n = strlen(buf);
//     if (n > 0 && buf[n - 1] == '\n') buf[n - 1] = '\0';
//     return 1;
// }
// int read_line(char *buf, size_t buflen) {
//     while (1) {
//         if (fgets(buf, buflen, stdin) == NULL) {
//             if (errno == EINTR) {
//                 clearerr(stdin);
//                 continue;
//             }
//             return 0; // real EOF or error
//         }
//         size_t n = strlen(buf);
//         if (n > 0 && buf[n - 1] == '\n') buf[n - 1] = '\0';
//         return 1;
//     }
// }
int read_line(char *buf, size_t buflen) {
    while (1) {
        if (fgets(buf, buflen, stdin) == NULL) {
            if (errno == EINTR) {
                clearerr(stdin);
                // a signal fired - run cleanup and reprint prompt
                cleanup_done_jobs();
                print_prompt();
                continue;
            }
            return 0;
        }
        size_t n = strlen(buf);
        if (n > 0 && buf[n - 1] == '\n') buf[n - 1] = '\0';
        return 1;
    }
}



/*
    Copies a line of input by detecting boundaries of string literals
        and filling in their whitespace such that whitespace tokenization
        is safe.

    Args:
        `char *dest`: The memory location to put the modified copy.
        `char *src`: The memory location of the original input.
*/
void make_copy_fill_quoted_whitespace(char *dest, char *src) {

    // whether we are currently inside a literal
    int inside_quotes = 0;

    // which kind of quoting applies to that literal, \0 as placeholder
    char curr_quote = '\0';

    // whether the next character is an escaped character
    int escaped = 0;

    for (int i = 0; i < (int) strlen(src); ++i) {

        // if we just saw a backslash, continue normally
        if (escaped) {
            dest[i] = src[i];
            escaped = 0;
            continue;
        }

        // check for escape inside of double quotes - single quotes don't use it
        if (src[i] == '\\' && inside_quotes && curr_quote == '"') {
            escaped = 1;
            dest[i] = src[i];
            continue;
        }

        // outside quotes, enter double quote if not escaped
        if (!inside_quotes && src[i] == '"') {
            inside_quotes = !inside_quotes; // switch on/off
            curr_quote = '"'; // inside double quotes
            dest[i] = src[i]; // copy character
        }
        // outside quotes, enter single quote
        else if (!inside_quotes && src[i] == '\'') {
            inside_quotes = !inside_quotes; // switch on/off
            curr_quote = '\''; // inside double quotes
            dest[i] = src[i]; // copy character
        }
        // inside quotes, closing double quote properly
        else if (inside_quotes && src[i] == '"' && curr_quote == '"') {
            inside_quotes = !inside_quotes;
            curr_quote = '\0';
            dest[i] = src[i];
        }
        // inside quotes, closing single quote properly
        else if (inside_quotes && src[i] == '\'' && curr_quote == '\'') {
            inside_quotes = !inside_quotes;
            curr_quote = '\0';
            dest[i] = src[i];
        }
        else if (src[i] == ' ' && inside_quotes) {
            dest[i] = '\x01'; // replace whitespace inside quotes
        }
        else if (src[i] == '\t' && inside_quotes) {
            dest[i] = '\x02'; // same
        }
        else if (src[i] == '\n' && inside_quotes) {
            dest[i] = '\x03'; // same
        }
        else {
            dest[i] = src[i]; // regular copy
        }
    }
    dest[strlen(src)] = '\0'; // terminate string
}



/*
    Cleans up a token src and puts the result in dest. Removes quotes,
        reintroduces whitespace as was input into a literal, and handles
        ANSI escape sequences inside quoted literals.

    Args:
        `char *dest`: The memory location to put the clean result.
        `char *src`: The memory location of the original token.
*/
void whitespace_quotes_escapes(char *dest, char *src) {

    // check if this token is quoted, and remove quotes if so
    if (
        (src[0] == '"' && src[strlen(src) - 1] == '"')
        ||
        (src[0] == '\'' && src[strlen(src) - 1] == '\'')
    ) {
        int j = 0;
        for (int i = 1; i < (int) strlen(src) - 1; ++i) {
            if (src[i] == '\x01') {
                dest[j++] = ' '; // reintroduce whitespace inside quotes
            }
            else if (src[i] == '\x02') {
                dest[j++] = '\t'; // same
            }
            else if (src[i] == '\x03') {
                dest[j++] = '\n'; // same
            }
            // handle all ANSI escape sequences inside string literals
            else if (src[i] == '\\' && i < (int) strlen(src) - 1) {
                switch (src[i+1]) {
                    case 'a':
                        dest[j++] = '\a';
                        break;
                    case 'b':
                        dest[j++] = '\b';
                        break;
                    case 't':
                        dest[j++] = '\t';
                        break;
                    case 'n':
                        dest[j++] = '\n';
                        break;
                    case 'v':
                        dest[j++] = '\v';
                        break;
                    case 'f':
                        dest[j++] = '\f';
                        break;
                    case 'r':
                        dest[j++] = '\r';
                        break;
                    case 'e':
                        dest[j++] = '\e';
                        break;
                    default:
                        dest[j++] = '\\';
                        --i;
                }
                ++i;
            }
            else {
                dest[j++] = src[i]; // regular copy
            }
        }
        dest[j] = '\0'; // terminate string
    }
    else {
        strcpy(dest, src); // normal behavior if "normal" token
    }
}



/*
    Wraps shell special characters outside of quotes with spaces so that
    strtok() will split them as separate tokens.

    Args:
        `char *dst`: Destination string to put the command with wrapped ops.
        `const char *src`: Source string to read command from.
*/
static void isolate_operators(char *dst, const char *src) {
    char out[MAX_LINE];
    int i = 0;
    char in_quote = 0;

    for (const char *p = src; *p; ++p) {
        // track quote state
        if ((*p == '\'' || *p == '"') && !in_quote) {
            in_quote = *p;
        } else if (*p == in_quote) {
            in_quote = 0;
        }

        if (in_quote) {
            out[i++] = *p;
            continue;
        }

        // multi-char operators first (order matters)
        if (p[0] == '>' && p[1] == '>') {       // >>
            out[i++] = ' '; out[i++] = '>'; out[i++] = '>'; out[i++] = ' ';
            ++p;
        } else if (p[0] == '2' && p[1] == '>') { // 2>
            out[i++] = ' '; out[i++] = '2'; out[i++] = '>'; out[i++] = ' ';
            ++p;
        } else if (p[0] == '<' && p[1] == '<') { // <<
            out[i++] = ' '; out[i++] = '<'; out[i++] = '<'; out[i++] = ' ';
            ++p;
        } else if (p[0] == '&' && p[1] == '&') { // &&
            out[i++] = ' '; out[i++] = '&'; out[i++] = '&'; out[i++] = ' ';
            ++p;
        } else if (p[0] == '|' && p[1] == '|') { // ||
            out[i++] = ' '; out[i++] = '|'; out[i++] = '|'; out[i++] = ' ';
            ++p;
        } else if (*p == '>' || *p == '<' || *p == '|' || *p == '&') {
            out[i++] = ' '; out[i++] = *p; out[i++] = ' ';
        } else {
            out[i++] = *p;
        }
    }

    out[i] = '\0';
    memcpy(dst, out, i + 1);
}



/*
    Parses a line of input sent from the bash terminal using strtok().

    Args:
        `char *line`: The line, with terminating newline character removed.
    Returns:
        `char **`: The command and its associated arguments, assumed to be
            whitespace-separated. Index 0 is assumed to be the command.
*/
char **parse_line(char *line) {

    // make copy of line because tokenizing destroys it
    char line_copy[MAX_LINE];
    make_copy_fill_quoted_whitespace(line_copy, line);
    isolate_operators(line_copy, line_copy); // wrap redirect, pipe, etc with ' '

    // initiate tokenization process
    char *token = strtok(line_copy, " \t\n");

    // count and store arguments
    int num_args = 0;
    char **args;
    args = (char **) malloc(MAX_ARGS * sizeof(char *));

    while (token != NULL && num_args < MAX_ARGS - 1) {
        // malloc enough space to copy token to args array
        args[num_args] = malloc(strlen(token) + 1);

        // copy token to args array, remove quoting and re-introduce whitespace
        whitespace_quotes_escapes(args[num_args], token);
        ++num_args; // count arg

        token = strtok(NULL, " \t\n"); // get next token
    }

    // terminate with NULL for later readability
    args[num_args] = NULL;

    return args;
}



/*
    Determines whether the given command is a built-in or external.

    Args:
        `const char *command`: A string containing the command to be checked.
    Returns:
        `int`: 1 if the command is a built-in, else 0.
*/
int is_builtin(const char *command) {

    if (command == NULL) return 0;

    for (int i = 0; builtins[i] != NULL; ++i) {    //loop until NULL
        if (strcmp(command, builtins[i]) == 0) {
            return 1;
        }
    }

    return 0;

}



/*
    Executes a built-in command (exit, cd, pwd).

    Args:
        `char **args`: The command (index 0) and its associated arguments.
    Returns:
        `int`: 1 to continue running.
*/
int execute_builtin_command(char **args) {

    int saved_stdout = dup(STDOUT_FILENO);

    // strip < > >> 2> from argv and store filenames in redir
    Redirection redir;
    init_redirection(&redir); // init redirection data
    args = strip_redirections(args, &redir); // clean args and parse redirects
    if (args == NULL || args[0] == NULL) { // if nothing to do
        free_redirection(&redir); // free things
        free_args(args);
        return 1; // and give up
    }

    // attempt to set input, output, err, etc.
    if (apply_redirections(&redir) < 0) {
        _exit(1);
    }

    char *command = args[0];
    char *path = args[1];

    if (strcmp(command, "exit") == 0) {
        printf("\nGoodbye!\n\n");
        return 0; // exit safely
    }
    else if (strcmp(command, "cd") == 0) {
        if (path == NULL) { // cd sends you to ~ if you don't supply an arg
            path = getenv("HOME");
        }
        if (chdir(path) != 0) { // change directory now
            fprintf(stderr, "myshell: cd: %s: No such file or directory\n", path);
        }
    }
    else if (strcmp(command, "pwd") == 0) {
        char cwd[MAX_DIR_LEN]; // space to store cwd
        getcwd(cwd, sizeof(cwd));
        fflush(stderr);
        printf("%s\n", cwd);
        fflush(stdout); // flush before restoring stdout
    }
    else if (strcmp(command, "jobs") == 0) {
        builtin_jobs();
    }
    else if (strcmp(command, "fg") == 0) {
        if (args[1] == NULL) {
            printf("myshell: fg: usage: fg %%jobid\n");
        } else {
            int job_id = 0;
            if (args[1][0] == '%') job_id = atoi(args[1] + 1);
            else job_id = atoi(args[1]);
            builtin_fg(job_id);
        }
    }
    else if (strcmp(command, "bg") == 0) {
        if (args[1] == NULL) {
            printf("myshell: bg: usage: bg %%jobid\n");
        } else {
            int job_id = 0;
            if (args[1][0] == '%') job_id = atoi(args[1] + 1);
            else job_id = atoi(args[1]);
            builtin_bg(job_id);
        }
    }

    // return control to user
    dup2(saved_stdout, STDOUT_FILENO);
    close(saved_stdout);

    return 1; // continue running
}



/*
    Determines whether the given string contains glob metacharacters. Ignores those chars if quoted.

    Args:
        `const char *s`: A string to be checked for glob metacharacters.
    Returns:
        `int`: 1 if the string contains glob metacharacters, else 0.
*/
static int has_glob_chars(const char *s) {

    // check if this token is quoted
    size_t len = strlen(s);
    if (len >= 2) {
        char first = s[0], last = s[len - 1];
        if (first == '\'' && last == '\'') {
            return 0; // if quoted, ignore glob chars
        }
        else if (first == '\"' && last == '\"') {
            return 0; // same for ""
        }
    }

    // Minimal: *, ?, [ are glob metacharacters
    for (; *s; ++s) {
        if (*s == '*' || *s == '?' || *s == '[') return 1;
    }
    return 0;
}



/*
    Expands glob patterns in the argument array.

    Args:
        `char **args`: The command (index 0) and its associated arguments.
    Returns:
        `char **`: A new argument array with glob patterns expanded, or the
            original if no glob patterns were found or if an error occurred.
*/
char **expand_globs(char **args) {
    if (args == NULL) return NULL;

    int index = 0;                       // how many args we have written to out[]
    char **new_args = malloc(MAX_ARGS * sizeof(char *));
    if (!new_args) return NULL;

    for (int i = 0; args[i] != NULL; ++i) {
        const char *tok = args[i];

        // Stop if we have no room left
        if (index >= MAX_ARGS - 1) break;

        if (!has_glob_chars(tok)) { // no glob chars -> copy literal token
            new_args[index] = malloc(strlen(tok) + 1);
            if (!new_args[index]) break;
            strcpy(new_args[index], tok);
            index++;
            continue;
        }

        // Expand glob pattern
        glob_t g = {0};  // initialize glob_t structure to 0

        int flag = glob(tok, 0, NULL, &g); // 0 means no special flags

        if (flag == 0) {
            // append matches
            for (size_t k = 0; k < g.gl_pathc && index < MAX_ARGS - 1; ++k) {
                const char *match = g.gl_pathv[k];
                new_args[index] = malloc(strlen(match) + 1);
                if (!new_args[index]) break;
                strcpy(new_args[index], match);
                index++;
            }
        } else {
            // No matches -> keep literal token
            new_args[index] = malloc(strlen(tok) + 1);
            if (new_args[index]) {
                strcpy(new_args[index], tok);
                index++;
            }
        }

        globfree(&g);
    }

    new_args[index] = NULL;
    return new_args;
}



/*
    Initializes the values of a `Redirection` struct.

    Args:
        `Redirection *redir`: A pointer to the Redirection struct to initialize.
*/
void init_redirection(Redirection *redir) {
    redir->input_file = NULL;
    redir->output_file = NULL;
    redir->error_file = NULL;
    redir->append_output = 0;
}



/*
    Frees the memory of a `Redirection` struct.

    Args:
        `Redirection *redir`: A pointer to the Redirection struct whose memory
            should be freed.
*/
void free_redirection(Redirection *redir) {
    free(redir->input_file);
    free(redir->output_file);
    free(redir->error_file);
}



/*
    Cleans arguments by removing redirection tokens and interpreting
        redirection-related arguments as such.

    Args:
        char **args: The argument array sent by the user.
        Redirection *redir: A pointer to a Redirection struct where pointers
            to redirection file names can be stored.
*/
char **strip_redirections(char **args, Redirection *redir) {

    // allocate a new array of strings to store the clean args
    char **clean_args = malloc(MAX_ARGS * sizeof(char *));
    if (clean_args == NULL) return NULL; // avoid null pointers

    int j = 0;

    // iterate over args array, converting it to clean args (at most MAX_ARGS)
    for (int i = 0; args[i] != NULL && j < MAX_ARGS - 1; ++i) {
        if (strcmp(args[i], "<") == 0) { // if we find the <
            if (args[i + 1] != NULL) { // look for input target
                redir->input_file = strdup(args[i + 1]); // copy
                i++; // next arg
            }
        }
        else if (strcmp(args[i], ">") == 0) { // if we find the >
            if (args[i + 1] != NULL) { // look for output target
                redir->output_file = strdup(args[i + 1]); // copy
                redir->append_output = 0; // set to not append
                i++; // next arg
            }
        }
        else if (strcmp(args[i], ">>") == 0) { // if we find the >>
            if (args[i + 1] != NULL) { // look for output target
                redir->output_file = strdup(args[i + 1]); // copy
                redir->append_output = 1; // set to append
                i++; // next arg
            }
        }
        else if (strcmp(args[i], "2>") == 0) { // if we find the 2>
            if (args[i + 1] != NULL) { // look for err target
                redir->error_file = strdup(args[i + 1]); // copy
                i++; // next arg
            }
        }
        else { // any non-redirection related arg
            clean_args[j] = malloc(strlen(args[i]) + 1);
            if (clean_args[j] == NULL) { // avoid null pointers
                clean_args[j] = NULL;
                return clean_args; // reached end of list
            }
            strcpy(clean_args[j], args[i]); // else copy arg to clean array
            j++;
        }
    }

    clean_args[j] = NULL; // ensure null terminator
    return clean_args;
}



/*
    Makes subdirectories as needed when redirecting output/error.

    Args:
        `const char *filepath`: The filepath whose subdirectories may need to
            be created.
    Returns:
        `int`: 0 if success, else -1.
*/
int mkdir_p(const char *filepath) {
    char *path = strdup(filepath); // copy to avoid modifying original
    if (!path) return -1;

    // walk through each subdir, stopping before the filename
    for (char *p = path + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0'; // temporarily truncate
            // mkdir up to this slash
            if (mkdir(path, 0755) < 0 && errno != EEXIST) {
                free(path);
                return -1;
            }
            *p = '/'; // restore /
        }
    }
    free(path);
    return 0;
}



/*
    Applies redirection choices parsed when cleaning the arguments.

    Args:
        `const Redirection *redir`: Pointer to Redirection struct holding the
            options (input, output, err, flag for append/clear).
    Returns:
        `int`: 0 if success, -1 if failure.
*/
int apply_redirections(const Redirection *redir) {
    int fd; // file descriptor

    // if we are getting input by redirection
    if (redir->input_file != NULL) {
        fd = open(redir->input_file, O_RDONLY); // open that file to read
        if (fd < 0) { // can't open file
            perror(redir->input_file); // error on this
            return -1;
        }
        if (dup2(fd, STDIN_FILENO) < 0) { // try to assign stdin to fd
            perror("dup2"); // error on dup2 process
            close(fd); // ensure file is closed
            return -1;
        }
        close(fd); // close input file
    }

    // if we are redirecting output
    if (redir->output_file != NULL) {
        int flags = O_WRONLY | O_CREAT;
        if (redir->append_output) // if we are appending
            flags |= O_APPEND; // also flip the bits on for that
        else // otherwise ask permission to clear the file
            flags |= O_TRUNC;
        
        // create subdirs if needed
        mkdir_p(redir->output_file);
        
        // potentially read/write/create, use standard perms if needed
        fd = open(redir->output_file, flags, 0644);
        if (fd < 0) { // can't open file
            perror(redir->output_file); // error on this
            return -1;
        }
        if (dup2(fd, STDOUT_FILENO) < 0) { // try to assign stdout to fd
            perror("dup2"); // error if needed
            close(fd); // ensure file closed
            return -1;
        }
        close(fd); // close output file
    }

    // if redirecting error messages
    if (redir->error_file != NULL) {

        // make subdirs if needed
        mkdir_p(redir->error_file);
        
        // potentially read/write/create/clear, standard perms
        fd = open(redir->error_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) { // can't open file
            perror(redir->error_file); // error on this
            return -1;
        }
        if (dup2(fd, STDERR_FILENO) < 0) { // try to assign stderr to fd
            perror("dup2"); // error if needed
            close(fd); // ensure file closed
            return -1;
        }
        close(fd); // close err file
    }

    return 0;
}

/*
    Executes an external command.

    Args:
        `char **args`: The command (index 0) and its associated arguments.
*/
void execute_external_command(char **args) {

    // null checking
    if (args == NULL || args[0] == NULL) {
        return;
    }
    
    // max line length allowed
    char cmdline[MAX_LINE] = {0};

    // paste args together separated with spaces
    for (int i = 0; args[i] != NULL; ++i) {
        if (i > 0) {
            strncat(cmdline, " ", MAX_LINE - strlen(cmdline) - 1);
        }
        strncat(cmdline, args[i], MAX_LINE - strlen(cmdline) - 1);
    }

    // strip < > >> 2> from argv and store filenames in redir
    Redirection redir;
    init_redirection(&redir); // init redirection data

    char **clean_args = strip_redirections(args, &redir); // clean args and parse redirects
    if (clean_args == NULL || clean_args[0] == NULL) { // if nothing to do
        free_redirection(&redir); // free things
        free_args(args);
        free_args(clean_args);
        return; // and give up
    }

    // expand globs in args
    char **expanded_args = expand_globs(clean_args);

    // fork here to execute the external command in a new process
    pid_t pid = fork();

    // pid being -1 means an error occurred creating the fork
    if (pid < 0) {
        perror("fork");
        free_redirection(&redir);
        free_args(clean_args);
        free_args(expanded_args);
        return;
    }

    if (pid == 0) {

        // Child: put itself in its own process group
        setpgid(0, 0);

        // Give child the terminal if interactive
        // if (isatty(STDIN_FILENO)) {
        //     tcsetpgrp(STDIN_FILENO, getpid());
        // }

        signal(SIGINT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGTTOU, SIG_DFL);
        signal(SIGTTIN, SIG_DFL);

        // Apply redirections
        if (apply_redirections(&redir) < 0) {
            free_redirection(&redir);
            free_args(clean_args);
            free_args(expanded_args);
            _exit(1);
        }
        
        // execute
        execvp(expanded_args[0], expanded_args);

        fprintf(stderr, "myshell: %s: command not found\n", expanded_args[0]);

        free_redirection(&redir);
        free_args(clean_args);
        free_args(expanded_args);
        _exit(127);
    }

    // Parent: also try to place child in its own process group
    setpgid(pid, pid);

    foreground_pid = pid;
    current_child_pid = pid;

    if (isatty(STDIN_FILENO)) {
        tcsetpgrp(STDIN_FILENO, pid);
    }

    int status = 0;
    if (waitpid(pid, &status, WUNTRACED) < 0) {
        perror("waitpid");
    }

    // Restore terminal control to shell
    if (isatty(STDIN_FILENO)) {
        tcsetpgrp(STDIN_FILENO, shell_pid);
    }

    foreground_pid = 0;
    current_child_pid = 0;

    // If stopped with Ctrl+Z, add to jobs table
    if (WIFSTOPPED(status)) {
        int jid = add_job_phase4(pid, pid, cmdline, 0);
        Job *job = find_job_by_id(jid);
        if (job != NULL) {
            job->status = JOB_STOPPED;
            printf("\n[%d] Stopped %s\n", job->job_id, job->command_line);
            fflush(stdout);
        }
    }

    free_redirection(&redir);
    free_args(clean_args);
    free_args(expanded_args);
}



/*
    Frees the memory allocated to the argument array.

    Args:
        char **args: The command (index 0) and its associated arguments.
*/
void free_args(char **args) {
    if (args == NULL) { // no args, nothing to do
        return;
    }

    // free one-by-one
    for (int i = 0; args[i] != NULL; ++i) {
        free(args[i]);
    }

    // now free ref to array
    free(args);
}
