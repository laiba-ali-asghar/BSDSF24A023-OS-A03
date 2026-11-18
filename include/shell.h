#ifndef SHELL_H
#define SHELL_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <ctype.h>
#include <fcntl.h>
#include <dirent.h>
#include <readline/readline.h>
#include <readline/history.h>

#define MAX_LEN 512
#define MAXARGS 20
#define ARGLEN 256
#define PROMPT "FCIT> "
#define HISTORY_SIZE 20
#define MAX_JOBS 20
#define MAX_CMDS 20
#define MAX_BLOCK 20

// Function prototypes
char* read_cmd(char* prompt, FILE* fp);
char** tokenize(char* cmdline);
int execute(char** arglist);
int handle_builtin(char** arglist);
void add_to_history(char* cmdline);
void print_history();
char* get_history(int n);
void init_readline();
void reap_jobs();
void print_jobs();
void expand_variables(char** arglist);
int handle_assignment(char** arglist);
void print_variables();

#endif // SHELL_H
