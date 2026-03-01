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

#include <string.h>

#include "shell.h"



/*
    Entry point to the program. Handles REPL loop.
*/
int main() {

    int exit_status = 1;
    char line[MAX_LINE];

    while (exit_status) {
        print_prompt();

        if (!read_line(line, sizeof(line))) break;  // Ctrl+D exits
        if (line[0] == '\0') continue;

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

        free_args(args);
    }

    return 0;
}