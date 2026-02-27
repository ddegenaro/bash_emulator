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

#include "shell.h"

const char *builtins[] = {"exit", "cd", "pwd", NULL};

/*
    Prints the bash prompt.
*/
void print_prompt() {
    printf("myshell -> ");
    fflush(stdout);
}



/*
    Reads a line of input sent from the bash terminal.

    Returns:
        char *: The line, with the terminating newline character removed.
*/
char *read_line() {
    // scan MAX_LINE chars from stdin
    char *line = fgets("%s", MAX_LINE, stdin);
    line[strlen(line) - 1] = '\0'; // cut off \n char
    return line;
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
    strcpy(line_copy, line);

    // initiate tokenization process
    char *token = strtok(line_copy, " \t\n");

    // count and store arguments
    int num_args = 0;
    char **args;
    args = (char **) malloc(MAX_ARGS * sizeof(char *));

    while (token != NULL && num_args < MAX_ARGS - 1) {
        // malloc enough space to copy token to args array
        args[num_args] = malloc(strlen(token) + 1);
        strcpy(args[num_args], token); // copy token to args array
        ++num_args; // count arg
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

    // check command against each builtin
    int builtins_size = (int) sizeof(builtins) / sizeof(builtins[0]);
    for (int i = 0; i < builtins_size; ++i) {
        if (strcmp(command, builtins[i]) == 0) {
            return 1; // is a built-in
        }
    }

    return 0; // not a built-in

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
        printf("Goodbye!");
        return 0; // exit safely
    }
    else if (strcmp(command, "cd") == 0) {
        if (path == NULL) { // cd sends you to ~ if you don't supply an arg
            path = getenv("HOME");
        }
        if (chdir(path) != 0) { // change directory now
            fprintf(stderr, "myshell: cd: %s: No such file or directory", path);
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
    Executes an external command.

    Args:
        char **args: The command (index 0) and its associated arguments.
*/
void execute_external_command(char **args) {
    // TODO
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

