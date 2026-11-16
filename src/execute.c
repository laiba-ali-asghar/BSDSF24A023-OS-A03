#include "shell.h"

// Feature-5: Separate the redirections from the real arguments.
// Example: ["sort", "<", "in.txt", ">", "out.txt"]
//   -> cmd = ["sort"], infile = "in.txt", outfile = "out.txt"
void parse_redirection(char** args, char** cmd, char** infile, char** outfile) {
    int j = 0;
    *infile = NULL;
    *outfile = NULL;
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], "<") == 0 && args[i + 1] != NULL) {
            *infile = args[i + 1];
            i++;                       // skip the file name
        } else if (strcmp(args[i], ">") == 0 && args[i + 1] != NULL) {
            *outfile = args[i + 1];
            i++;
        } else {
            cmd[j] = args[i];
            j++;
        }
    }
    cmd[j] = NULL;
}

// Runs in the CHILD (after fork, before exec): connect stdin/stdout to files
void setup_redirection(char* infile, char* outfile) {
    if (infile != NULL) {
        int fd = open(infile, O_RDONLY);
        if (fd < 0) { perror(infile); exit(1); }
        dup2(fd, STDIN_FILENO);        // stdin now reads from the file
        close(fd);
    }
    if (outfile != NULL) {
        int fd = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) { perror(outfile); exit(1); }
        dup2(fd, STDOUT_FILENO);       // stdout now writes to the file
        close(fd);
    }
}

// Runs one simple command (with optional < and >)
int run_single(char** arglist) {
    char* cmd[MAXARGS + 1];
    char *infile, *outfile;
    parse_redirection(arglist, cmd, &infile, &outfile);
    if (cmd[0] == NULL) return 0;

    int status;
    int cpid = fork();
    switch (cpid) {
        case -1:
            perror("fork failed");
            exit(1);
        case 0: // Child process
            setup_redirection(infile, outfile);
            execvp(cmd[0], cmd);
            perror("Command not found");
            exit(1);
        default: // Parent process
            waitpid(cpid, &status, 0);
            return 0;
    }
}

// Runs: left_cmd | right_cmd
int run_pipe(char** left, char** right) {
    char* lcmd[MAXARGS + 1];
    char* rcmd[MAXARGS + 1];
    char *lin, *lout, *rin, *rout;
    parse_redirection(left, lcmd, &lin, &lout);
    parse_redirection(right, rcmd, &rin, &rout);
    if (lcmd[0] == NULL || rcmd[0] == NULL) {
        printf("Invalid pipe command\n");
        return 0;
    }

    int fd[2];                 // fd[0] = read end, fd[1] = write end
    if (pipe(fd) < 0) { perror("pipe"); return 0; }

    int pid1 = fork();
    if (pid1 == 0) {           // LEFT child: writes into the pipe
        dup2(fd[1], STDOUT_FILENO);
        close(fd[0]);
        close(fd[1]);
        setup_redirection(lin, lout);
        execvp(lcmd[0], lcmd);
        perror("Command not found");
        exit(1);
    }

    int pid2 = fork();
    if (pid2 == 0) {           // RIGHT child: reads from the pipe
        dup2(fd[0], STDIN_FILENO);
        close(fd[0]);
        close(fd[1]);
        setup_redirection(rin, rout);
        execvp(rcmd[0], rcmd);
        perror("Command not found");
        exit(1);
    }

    // PARENT: must close both ends, otherwise the right child never sees end-of-input
    close(fd[0]);
    close(fd[1]);
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);
    return 0;
}

int execute(char* arglist[]) {
    // Is there a pipe symbol?
    int p = -1;
    for (int i = 0; arglist[i] != NULL; i++) {
        if (strcmp(arglist[i], "|") == 0) {
            p = i;
            break;
        }
    }

    if (p == -1) {
        return run_single(arglist);
    }

    // Split into left part (before |) and right part (after |)
    char* left[MAXARGS + 1];
    for (int i = 0; i < p; i++) {
        left[i] = arglist[i];
    }
    left[p] = NULL;
    char** right = &arglist[p + 1];
    return run_pipe(left, right);
}