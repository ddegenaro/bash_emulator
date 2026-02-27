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

        // get a line
        strcpy(line, read_line());
        if (line[0] == '\0') { // check for empty input
            continue;
        }

        // parse it into argument list
        char **args = parse_line();

        // check if built-in, execute appropriate behavior
        if (is_builtin(args[0])) { // args[0] is command
            exit_status = execute_builtin_command(args);
        }
        else {
            execute_external_command(args);
        }
    }

    return 0;
}