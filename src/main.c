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
// Feature-7: returns the exit status (0 = success)
int run_command(char* cmd) {
    char** arglist = tokenize(cmd);
    if (arglist == NULL) return 0;        // empty command

    int status = 0;
    if (handle_builtin(arglist) == 0) {
        status = execute(arglist);
    }

    // Free the memory allocated by tokenize()
    for (int i = 0; arglist[i] != NULL; i++) {
        free(arglist[i]);
    }
    free(arglist);
    return status;
}

// Feature-7: remove spaces from the start and end of a string
char* trim(char* s) {
    while (*s == ' ' || *s == '\t') s++;
    int len = strlen(s);
    while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t')) {
        s[len - 1] = '\0';
        len--;
    }
    return s;
}

// Feature-7: if-then-else-fi
// condition = the command written after "if"
void handle_if(char* condition) {
    char* then_cmds[MAX_BLOCK];   // commands between then and else/fi
    char* else_cmds[MAX_BLOCK];   // commands between else and fi
    int then_count = 0, else_count = 0;
    int mode = 0;                 // 0 = waiting for "then", 1 = in then-block, 2 = in else-block
    int ok = 1;

    // Keep reading lines until we see "fi"
    while (1) {
        char* line = readline("> ");
        if (line == NULL) {                       // Ctrl+D before fi
            printf("Error: missing 'fi'\n");
            ok = 0;
            break;
        }
        char* t = trim(line);

        if (strcmp(t, "then") == 0) {
            mode = 1;
        } else if (strcmp(t, "else") == 0) {
            mode = 2;
        } else if (strcmp(t, "fi") == 0) {
            free(line);
            break;                                // end of the if-block
        } else if (t[0] == '\0') {
            // empty line: ignore
        } else if (mode == 1 && then_count < MAX_BLOCK) {
            then_cmds[then_count++] = strdup(t);
        } else if (mode == 2 && else_count < MAX_BLOCK) {
            else_cmds[else_count++] = strdup(t);
        } else if (mode == 0) {
            printf("Error: expected 'then'\n");
        }
        free(line);
    }

    if (ok && mode == 0) {
        printf("Error: missing 'then'\n");
        ok = 0;
    }

    if (ok) {
        int status = run_command(condition);     // run the if-command, get its exit code
        if (status == 0) {
            for (int i = 0; i < then_count; i++) run_command(then_cmds[i]);   // success
        } else {
            for (int i = 0; i < else_count; i++) run_command(else_cmds[i]);   // failure
        }
    }

    // free the stored lines
    for (int i = 0; i < then_count; i++) free(then_cmds[i]);
    for (int i = 0; i < else_count; i++) free(else_cmds[i]);
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

        // Feature-7: a line starting with "if " begins an if-block
        char* t = trim(cmdline);
        if (strncmp(t, "if ", 3) == 0) {
            handle_if(t + 3);          // everything after "if " is the condition
            free(cmdline);
            continue;
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