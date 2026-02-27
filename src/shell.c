#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_ARGS 20
#define MAX_LINE 256

const char *builtins[] = {"exit", "cd", "pwd", NULL};

/*

*/
void print_prompt() {
    printf("myshell -> ");
    fflush(stdout);
}



/*

*/
char *read_line() {
    // scan MAX_LINE chars from stdin
    char *line = fgets("%s", MAX_LINE, stdin);
    line[strlen(line) - 1] = '\0'; // cut off \n char
    return line;
}



/*

*/
char **parse_line(char *line) {

    // make copy of line because tokenizing destroys it
    char *line_copy;
    strcpy(line_copy, line);

    // initiate tokenization process
    char *token = strtok(line_copy, " \t\n");

    // count and store arguments
    int num_args = 0;
    char **args;

    while (token != NULL && num_args < MAX_ARGS - 1) {
        args[num_args] = malloc(strlen(token) + 1);

        strcpy(args[num_args], token);

        ++num_args;
    }

    args[num_args] = NULL;
    free(line_copy);

    return args;
}



/*

*/
int is_builtin(const char *command) {

    for (int i = 0; i < sizeof(builtins) / sizeof(builtins[0]); ++i) {
        if (strcmp(command, builtins[i]) == 0) {
            return 1; // is a built-in
        }
    }

    return 0; // not a built-in

}

int execute_builtin_command(char *command, char **args) {

    char *path = args[1];

    if (strcmp(command, "exit") == 0) {
        printf("Goodbye!");
        _exit(0);
    }
    else if (strcmp(command, "cd") == 0) {
        if (path == NULL) { // cd sends you to ~ if you don't supply an arg
            path = getenv("HOME");
        }
        if (chdir(path) != 0) { // change directory now
            fprintf(stderr, "myshell: cd: %s: No such file or directory", path);
        }
        return 1; // continue running
    }
    else if (strcmp(command, "pwd") == 0) {
        char cwd[1024];
        getcwd(cwd, sizeof(cwd));
        printf("%s\n", cwd);
        return 1;
    }

}

void execute_external(char *command) {

}

void free_args(char **args) {
    if (args == NULL) {
        return;
    }
    for (int i = 0; args[i] != NULL; ++i) {
        free(args[i]);
    }
    free(args);
}

