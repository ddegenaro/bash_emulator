#include <string.h>

#include "shell.h"

int main() {

    int exit_status = 1;

    char line[MAX_LINE];

    while (exit_status) {
        print_prompt();

        strcpy(line, read_line());

        if (line[0] == '\0') {
            continue;
        }

        char **args = parse_line();

        if (is_builtin(args[0])) {
            execute_builtin(args);
        }
        else {
            execute_external(args);
        }
    }

    return 0;
}