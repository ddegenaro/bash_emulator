#include <string.h>

#include "shell.h"

int main() {

    int exit_status = 1;

    char line[256];
    char command[20];

    while (exit_status) {
        print_prompt();

        strcpy(line, read_line());

        if (line[0] == '\0') {
            continue;
        }

        parse_line(&line, &command);

        if (is_builtin(&command)) {
            execute_builtin(&command);
        }
        else {
            execute_external(&command);
        }
    }

    return 0;
}