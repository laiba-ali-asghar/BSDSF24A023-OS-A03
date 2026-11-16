#include "shell.h"

char* read_cmd(char* prompt, FILE* fp) {
    printf("%s", prompt);
    char* cmdline = (char*) malloc(sizeof(char) * MAX_LEN);
    int c, pos = 0;

    while ((c = getc(fp)) != EOF) {
        if (c == '\n') break;
        cmdline[pos++] = c;
    }

    if (c == EOF && pos == 0) {
        free(cmdline);
        return NULL; // Handle Ctrl+D
    }
    
    cmdline[pos] = '\0';
    return cmdline;
}

char** tokenize(char* cmdline) {
    // Edge case: empty command line
    if (cmdline == NULL || cmdline[0] == '\0' || cmdline[0] == '\n') {
        return NULL;
    }

    char** arglist = (char**)malloc(sizeof(char*) * (MAXARGS + 1));
    int argnum = 0;
    char* cp = cmdline;

    while (*cp != '\0' && argnum < MAXARGS) {
        while (*cp == ' ' || *cp == '\t') cp++;   // skip spaces
        if (*cp == '\0') break;

        char token[ARGLEN];
        int len = 0;

        if (*cp == '<' || *cp == '>' || *cp == '|') {
            // a special symbol is always a token on its own
            token[len++] = *cp;
            cp++;
        } else {
            // a normal word: read until a space or a special symbol
            while (*cp != '\0' && *cp != ' ' && *cp != '\t' &&
                   *cp != '<' && *cp != '>' && *cp != '|') {
                if (*cp == '"' || *cp == '\'') {
                    // quoted text: copy everything up to the closing quote (quotes removed)
                    char quote = *cp;
                    cp++;
                    while (*cp != '\0' && *cp != quote) {
                        if (len < ARGLEN - 1) token[len++] = *cp;
                        cp++;
                    }
                    if (*cp == quote) cp++;
                } else {
                    if (len < ARGLEN - 1) token[len++] = *cp;
                    cp++;
                }
            }
        }
        token[len] = '\0';
        arglist[argnum] = strdup(token);
        argnum++;
    }

    if (argnum == 0) {   // line had only spaces
        free(arglist);
        return NULL;
    }
    arglist[argnum] = NULL;
    return arglist;
}

// Feature-2: Built-in commands
// Returns 1 if the command was a built-in (and we ran it), 0 otherwise.
int handle_builtin(char** arglist) {
    char* cmd = arglist[0];

    if (strcmp(cmd, "exit") == 0) {
        printf("Shell exited.\n");
        exit(0);
    }

    if (strcmp(cmd, "cd") == 0) {
        char* dir = arglist[1];
        if (dir == NULL) {
            dir = getenv("HOME");   // plain "cd" goes to home
        }
        if (chdir(dir) != 0) {
            perror("cd");           // e.g. folder does not exist
        }
        return 1;
    }

    if (strcmp(cmd, "help") == 0) {
        printf("Built-in commands:\n");
        printf("  cd <dir>   Change directory\n");
        printf("  exit       Exit the shell\n");
        printf("  help       Show this help message\n");
        printf("  jobs       Show background jobs\n");
                printf("  history    Show command history\n");
        return 1;
    }

        if (strcmp(cmd, "history") == 0) {
        print_history();
        return 1;
    }

    if (strcmp(cmd, "jobs") == 0) {
        printf("Job control not yet implemented.\n");
        return 1;
    }

    return 0;   // not a built-in
}
// Feature-3: Command history
char* history[HISTORY_SIZE];   // stores the last 20 commands
int history_count = 0;         // how many commands are stored right now

void add_to_history(char* cmdline) {
    if (history_count == HISTORY_SIZE) {
        // History is full: delete the oldest one and shift the rest up
        free(history[0]);
        for (int i = 1; i < HISTORY_SIZE; i++) {
            history[i - 1] = history[i];
        }
        history_count--;
    }
    history[history_count] = strdup(cmdline);   // save a copy
    history_count++;
}

void print_history() {
    for (int i = 0; i < history_count; i++) {
        printf("%d  %s\n", i + 1, history[i]);
    }
}

// Returns the n-th command (n starts from 1), or NULL if n is invalid
char* get_history(int n) {
    if (n < 1 || n > history_count) {
        return NULL;
    }
    return history[n - 1];
}

// Feature-4: Tab completion with readline
char** command_list = NULL;   // names of all commands we can complete
int command_count = 0;

void add_command_name(char* name) {
    command_list = realloc(command_list, sizeof(char*) * (command_count + 1));
    command_list[command_count] = strdup(name);
    command_count++;
}

// Fill command_list with our built-ins + every program in the PATH folders
void build_command_list() {
    char* builtins[] = {"cd", "exit", "help", "jobs", "history"};
    for (int i = 0; i < 5; i++) {
        add_command_name(builtins[i]);
    }

    char* path = getenv("PATH");
    if (path == NULL) return;
    char* path_copy = strdup(path);           // strtok changes the string, so use a copy
    char* folder = strtok(path_copy, ":");     // PATH folders are separated by ':'
    while (folder != NULL) {
        DIR* dp = opendir(folder);
        if (dp != NULL) {
            struct dirent* entry;
            while ((entry = readdir(dp)) != NULL) {
                if (entry->d_name[0] != '.') {
                    add_command_name(entry->d_name);
                }
            }
            closedir(dp);
        }
        folder = strtok(NULL, ":");
    }
    free(path_copy);
}

// readline calls this again and again; each call returns the next match
char* command_generator(const char* text, int state) {
    static int index, len;
    if (state == 0) {          // first call for this Tab press: start from the beginning
        index = 0;
        len = strlen(text);
    }
    while (index < command_count) {
        char* name = command_list[index];
        index++;
        if (strncmp(name, text, len) == 0) {
            return strdup(name);   // readline frees it later
        }
    }
    return NULL;               // no more matches
}

// Called by readline when the user presses Tab
char** myshell_completion(const char* text, int start, int end) {
    if (start == 0) {
        // first word on the line -> complete a command name
        return rl_completion_matches(text, command_generator);
    }
    return NULL;   // other words -> readline's default: complete file names
}

void init_readline() {
    build_command_list();
    rl_attempted_completion_function = myshell_completion;
}