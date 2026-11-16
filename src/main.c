#include "shell.h"

// Feature-6: split a line at ';' (but not at a ';' inside quotes)
// Example: "echo a ; sleep 2 ; echo b" -> "echo a ", " sleep 2 ", " echo b"
int split_commands(char* line, char** parts) {
    int count = 0;
    char quote = 0;            // are we inside quotes? (0 = no)
    parts[count++] = line;
    for (char* p = line; *p != '\0'; p++) {
        if (quote != 0) {
            if (*p == quote) quote = 0;           // closing quote
        } else if (*p == '"' || *p == '\'') {
            quote = *p;                           // opening quote
        } else if (*p == ';' && count < MAX_CMDS) {
            *p = '\0';                            // end the current command here
            parts[count++] = p + 1;               // next command starts after ';'
        }
    }
    return count;
}

// Tokenize and run ONE command (built-in or external)
void run_command(char* cmd) {
    char** arglist = tokenize(cmd);
    if (arglist == NULL) return;          // empty command

    if (handle_builtin(arglist) == 0) {
        execute(arglist);
    }

    // Free the memory allocated by tokenize()
    for (int i = 0; arglist[i] != NULL; i++) {
        free(arglist[i]);
    }
    free(arglist);
}

int main() {
    char* cmdline;

    init_readline();   // Feature-4: set up Tab completion

    while (1) {
        reap_jobs();   // Feature-6: clean up finished background jobs (zombies)

        cmdline = readline(PROMPT);
        if (cmdline == NULL) break;       // Ctrl+D

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

        // Feature-6: run each ';'-separated command one after another
        char* parts[MAX_CMDS];
        int count = split_commands(cmdline, parts);
        for (int i = 0; i < count; i++) {
            run_command(parts[i]);
        }

        free(cmdline);
    }

    printf("\nShell exited.\n");
    return 0;
}