#include "shell.h"

int main() {
    char* cmdline;
    char** arglist;

    init_readline();   // Feature-4: set up Tab completion

    while ((cmdline = readline(PROMPT)) != NULL) {        
        // Feature-3: handle !n (re-run command number n)
        if (cmdline[0] == '!') {
            int n = atoi(cmdline + 1);       // the number after '!'
            char* old = get_history(n);
            if (old == NULL) {
                printf("No such command in history: %s\n", cmdline);
                free(cmdline);
                continue;
            }
            free(cmdline);
            cmdline = strdup(old);           // replace !n with the real command
            printf("%s\n", cmdline);         // show what is being run
        }

        // Feature-3: save the command in history (skip empty lines)
        if (cmdline[0] != '\0') {
                        add_to_history(cmdline);   // our own list (for history and !n)
            add_history(cmdline);      // readline's list (for Up/Down arrows)
        }
        if ((arglist = tokenize(cmdline)) != NULL) {
                       if (handle_builtin(arglist) == 0) {
                execute(arglist);
            }

            // Free the memory allocated by tokenize()
            for (int i = 0; arglist[i] != NULL; i++) {
                free(arglist[i]);
            }
            free(arglist);
        }
        free(cmdline);
    }

    printf("\nShell exited.\n");
    return 0;
}