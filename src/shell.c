/*************************************************************************
 Authors:        Dan DeGenaro, Tian Li (Net IDs: drd92, tl995)
 Date:           Feb 27, 2026
 Last Updated:   Feb 27, 2026
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
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#include "shell.h"

const char *builtins[] = {"exit", "cd", "pwd", NULL};

/*
    Prints the bash prompt.
*/
void print_prompt() {
    printf("myshell--> ");
    fflush(stdout);
}



/*
    Reads a line of input sent from the bash terminal.
    Args:
        char *buf: A buffer to store the line of input.
        size_t buflen: The length of the buffer.
*/
int read_line(char *buf, size_t buflen) {
    if (fgets(buf, buflen, stdin) == NULL) return 0; // EOF
    size_t n = strlen(buf);
    if (n > 0 && buf[n - 1] == '\n') buf[n - 1] = '\0';
    return 1;
}



/*

*/
void make_copy_fill_quoted_whitespace(char *dest, char *src) {

    int inside_quotes = 0;
    char curr_quote = '\0';
    int escaped = 0;

    for (int i = 0; i < (int) strlen(src); ++i) {

        if (escaped) {
            dest[i] = src[i];
            escaped = 0;
            continue;
        }

        // check for escape inside of double quotes
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

*/
void reintroduce_whitespace_remove_quotes(char *dest, char *src) {

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
    else{
        strcpy(dest, src);
    }
}



/*
    Parses a line of input sent from the bash terminal using strtok().

    Args:
        char *line: The line, with terminating newline character removed.
    Returns:
        char **: The command and its associated arguments, assumed to be
            whitespace-separated. Index 0 is assumed to be the command.
*/
char **parse_line(char *line) {

    // make copy of line because tokenizing destroys it
    char line_copy[MAX_LINE];
    make_copy_fill_quoted_whitespace(line_copy, line);

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
        reintroduce_whitespace_remove_quotes(args[num_args], token);
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
        const char *command: A string containing the command to be checked.
    Returns:
        int: 1 if the command is a built-in, else 0.
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
        char **args: The command (index 0) and its associated arguments.
    Returns:
        int: 1 to continue running.
*/
int execute_builtin_command(char **args) {

    char *command = args[0];
    char *path = args[1];

    if (strcmp(command, "exit") == 0) {
        printf("Goodbye!\n");
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
        printf("%s\n", cwd);
    }

    return 1; // continue running
}


/*
    Determines whether the given string contains glob metacharacters.

    Args:
        const char *s: A string to be checked for glob metacharacters.
    Returns:
        int: 1 if the string contains glob metacharacters, else 0.
*/
static int has_glob_chars(const char *s) {
    // Minimal: *, ?, [ are glob metacharacters
    for (; *s; ++s) {
        if (*s == '*' || *s == '?' || *s == '[') return 1;
    }
    return 0;
}

/*
    Expands glob patterns in the argument array.

    Args:
        char **args: The command (index 0) and its associated arguments.
    Returns:
        char **: A new argument array with glob patterns expanded, or the
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
    Executes an external command.

    Args:
        char **args: The command (index 0) and its associated arguments.
*/
void execute_external_command(char **args) {

    args = expand_globs(args);

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork\n");
        return;
    }

    if (pid == 0) {
        // child: run command
        execvp(args[0], args);
        // only if exec fails
        fprintf(stderr, "myshell: %s: command not found\n", args[0]);
        _exit(127);
    }

    // parent: wait
    int status;
    if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid\n");
    }

    free_args(args);
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
