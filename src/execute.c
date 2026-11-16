#include "shell.h"

// Feature-6: list of background jobs
int job_pids[MAX_JOBS];          // PID of each background job
char* job_cmds[MAX_JOBS];        // command text of each job
int job_count = 0;

void add_job(int pid, char** cmd) {
    if (job_count == MAX_JOBS) {
        printf("Too many background jobs\n");
        return;
    }
    // join the words back into one string, e.g. "sleep 10"
    char text[MAX_LEN] = "";
    for (int i = 0; cmd[i] != NULL; i++) {
        strcat(text, cmd[i]);
        strcat(text, " ");
    }
    job_pids[job_count] = pid;
    job_cmds[job_count] = strdup(text);
    job_count++;
    printf("[%d] %d\n", job_count, pid);   // like bash: [job number] PID
}

void remove_job(int pid) {
    for (int i = 0; i < job_count; i++) {
        if (job_pids[i] == pid) {
            printf("[Done] %s\n", job_cmds[i]);
            free(job_cmds[i]);
            // shift the rest of the list up by one
            for (int j = i + 1; j < job_count; j++) {
                job_pids[j - 1] = job_pids[j];
                job_cmds[j - 1] = job_cmds[j];
            }
            job_count--;
            return;
        }
    }
}

void print_jobs() {
    if (job_count == 0) {
        printf("No background jobs\n");
        return;
    }
    for (int i = 0; i < job_count; i++) {
        printf("[%d] %d  Running  %s\n", i + 1, job_pids[i], job_cmds[i]);
    }
}

// Reap (collect) every finished background child without blocking
void reap_jobs() {
    int status;
    int pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        remove_job(pid);
    }
}

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
// background = 1 means "don't wait" (the command ended with &)
int run_single(char** arglist, int background) {
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
            if (background) {
                add_job(cpid, cmd);           // remember it, and DON'T wait
            } else {
                waitpid(cpid, &status, 0);    // foreground: wait for it
            }
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
    // Feature-6: make a copy of the argument list, without a final "&"
    char* args[MAXARGS + 1];
    int n = 0;
    while (arglist[n] != NULL) {
        args[n] = arglist[n];
        n++;
    }
    args[n] = NULL;

    int background = 0;
    if (n > 0 && strcmp(args[n - 1], "&") == 0) {
        background = 1;
        args[n - 1] = NULL;      // remove the "&"
        n--;
    }
    if (n == 0) return 0;

    // Is there a pipe symbol?
    int p = -1;
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], "|") == 0) {
            p = i;
            break;
        }
    }

    if (p == -1) {
        return run_single(args, background);
    }

    // Split into left part (before |) and right part (after |)
    char* left[MAXARGS + 1];
    for (int i = 0; i < p; i++) {
        left[i] = args[i];
    }
    left[p] = NULL;
    char** right = &args[p + 1];
    return run_pipe(left, right);
}