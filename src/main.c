/*************************************************************************
 Authors:        Dan DeGenaro, Tian Li (Net IDs: drd92, tl995)
 Date:           Feb 27, 2026
 Last Updated:   Feb 27, 2026
 Purpose:        Runs REPL loop for bash emulation program.
 Program:        main.c
 Platform:       Linux, Solaris, BSD
 gcc Version:    gcc (GCC) 8.5.0 20210514 (Red Hat 8.5.0-28)
 Version:        1.0
*************************************************************************/

#include <stdio.h>
#include <string.h>

#include "shell.h"



/*
    Entry point to the program. Handles REPL loop.
*/
int main() {

    int exit_status = 1;
    char line[MAX_LINE];

    printf("\nmyshell 1.0\n\n");

    // REPL loop - execute continuously until exit_status is 0
    while (exit_status) {
        print_prompt();

        // read a line
        if (!read_line(line, sizeof(line))) break;  // Ctrl+D exits

        // check if not blank
        if (line[0] == '\0') continue;

        // parse the line, free args if none
        char **args = parse_line(line);
        if (args[0] == NULL) { 
            free_args(args); 
            continue; 
        }

        // check if built-in, execute appropriate behavior
        if (is_builtin(args[0])) { // args[0] is command
            exit_status = execute_builtin_command(args);
        }
        else {
            execute_external_command(args);
        }

        // cleanup
        free_args(args);
    }

    return 0;
}