/**
 * @file tsh.c
 * @brief A tiny shell program with job control
 *
 * Passed all traces.
 *
 * @author Kejia Qiang <kqiang@andrew.cmu.edu>
 */

#include "csapp.h"
#include "tsh_helper.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

/*
 * If DEBUG is defined, enable contracts and printing on dbg_printf.
 */
#ifdef DEBUG
/* When debugging is enabled, these form aliases to useful functions */
#define dbg_printf(...) printf(__VA_ARGS__)
#define dbg_requires(...) assert(__VA_ARGS__)
#define dbg_assert(...) assert(__VA_ARGS__)
#define dbg_ensures(...) assert(__VA_ARGS__)
#else
/* When debugging is disabled, no code gets generated for these */
#define dbg_printf(...)
#define dbg_requires(...)
#define dbg_assert(...)
#define dbg_ensures(...)
#endif

/* Function prototypes */
void eval(const char *cmdline);

void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);
void sigquit_handler(int sig);
void cleanup(void);

void wait_SIGCHLD(void);
int to_FG(jid_t job);
int to_BG(jid_t job);

/* Global Variables*/
volatile sig_atomic_t flag; // Global flag

/**
 * @brief Initialize global varaibles, job list and parse
 * parameters.
 *
 * Setup signal handlers, atexit functions
 *
 * Basic flow control
 */
int main(int argc, char **argv) {
    char c;
    char cmdline[MAXLINE_TSH]; // Cmdline for fgets
    bool emit_prompt = true;   // Emit prompt (default)

    // Redirect stderr to stdout (so that driver will get all output
    // on the pipe connected to stdout)
    if (dup2(STDOUT_FILENO, STDERR_FILENO) < 0) {
        perror("dup2 error");
        exit(1);
    }

    // Parse the command line
    while ((c = getopt(argc, argv, "hvp")) != EOF) {
        switch (c) {
        case 'h': // Prints help message
            usage();
            break;
        case 'v': // Emits additional diagnostic info
            verbose = true;
            break;
        case 'p': // Disables prompt printing
            emit_prompt = false;
            break;
        default:
            usage();
        }
    }

    // Create environment variable
    if (putenv("MY_ENV=42") < 0) {
        perror("putenv error");
        exit(1);
    }

    // Set buffering mode of stdout to line buffering.
    // This prevents lines from being printed in the wrong order.
    if (setvbuf(stdout, NULL, _IOLBF, 0) < 0) {
        perror("setvbuf error");
        exit(1);
    }

    // Initialize the job list
    init_job_list();

    // Register a function to clean up the job list on program termination.
    // The function may not run in the case of abnormal termination (e.g. when
    // using exit or terminating due to a signal handler), so in those cases,
    // we trust that the OS will clean up any remaining resources.
    if (atexit(cleanup) < 0) {
        perror("atexit error");
        exit(1);
    }

    // Install the signal handlers
    Signal(SIGINT, sigint_handler);   // Handles Ctrl-C
    Signal(SIGTSTP, sigtstp_handler); // Handles Ctrl-Z
    Signal(SIGCHLD, sigchld_handler); // Handles terminated or stopped child

    Signal(SIGTTIN, SIG_IGN);
    Signal(SIGTTOU, SIG_IGN);

    Signal(SIGQUIT, sigquit_handler);

    // Execute the shell's read/eval loop
    while (true) {
        if (emit_prompt) {
            printf("%s", prompt);

            // We must flush stdout since we are not printing a full line.
            fflush(stdout);
        }

        if ((fgets(cmdline, MAXLINE_TSH, stdin) == NULL) && ferror(stdin)) {
            perror("fgets error");
            exit(1);
        }

        if (feof(stdin)) {
            // End of file (Ctrl-D)
            printf("\n");
            return 0;
        }

        // Remove any trailing newline
        char *newline = strchr(cmdline, '\n');
        if (newline != NULL) {
            *newline = '\0';
        }

        // Evaluate the command line
        eval(cmdline);
    }

    return -1; // control never reaches here
}

/**
 * @brief Evaluate one command line
 *
 * It will distinguish fg and bg command, setup IO redirections and
 * create child process to execute it.
 *
 * NOTE: The shell is supposed to be a long-running process, so this function
 *       (and its helpers) should avoid exiting on error.  This is not to say
 *       they shouldn't detect and print (or otherwise handle) errors!
 */
void eval(const char *cmdline) {
    parseline_return parse_result;
    struct cmdline_tokens token;

    // Parse command line
    parse_result = parseline(cmdline, &token);

    if (parse_result == PARSELINE_ERROR || parse_result == PARSELINE_EMPTY) {
        return;
    }

    pid_t pid;
    sigset_t mask_all, mask_one, mask_prev;
    int in_fd, out_fd;

    in_fd = STDIN_FILENO;
    out_fd = STDOUT_FILENO;

    sigfillset(&mask_all);
    sigemptyset(&mask_one);
    sigaddset(&mask_one, SIGCHLD);

    if (token.builtin == BUILTIN_NONE) {
        // Not a builtin command
        // Block SIGCHLD to prevent race
        sigprocmask(SIG_BLOCK, &mask_all, &mask_prev);

        // Create child process to run user job
        pid = fork();
        if (pid < 0) {
            perror("Fork Error");
            strerror(errno);
        }

        if (pid == 0) {
            // Child process
            setpgid(0, 0);
            // Unblock all masks before pexecute cmd
            sigprocmask(SIG_SETMASK, &mask_prev, NULL);
            // Try to open and redirect to input output FD
            if (token.infile) {
                in_fd = open(token.infile, O_RDONLY);
                if (in_fd < 0) {
                    perror(token.infile);
                    strerror(errno);
                    exit(EXIT_FAILURE);
                }
                if (dup2(in_fd, STDIN_FILENO) < 0) {
                    perror("Redirect Error");
                    strerror(errno);
                    exit(EXIT_FAILURE);
                }
            }

            // Try to open and redirect to FD
            if (token.outfile) {
                out_fd = open(token.outfile, O_WRONLY | O_TRUNC | O_CREAT,
                              S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
                if (out_fd < 0) {
                    perror(token.outfile);
                    strerror(errno);
                    exit(EXIT_FAILURE);
                }
                if (dup2(out_fd, STDOUT_FILENO) < 0) {
                    perror("Redirect Error");
                    strerror(errno);
                    exit(EXIT_FAILURE);
                }
            }

            // Exectue command
            if (execvp(token.argv[0], token.argv) < 0) {
                if (token.infile)
                    close(in_fd);
                if (token.outfile)
                    close(out_fd);
                perror(cmdline);
                strerror(errno);
                exit(EXIT_FAILURE);
            }
            // Clear redirection and exit
            if (token.infile)
                close(in_fd);
            if (token.outfile)
                close(out_fd);
            exit(EXIT_SUCCESS);
        } else {
            // Parent Process
            // Block all signals to add job list
            sigprocmask(SIG_BLOCK, &mask_all, NULL);
            // Add process to job list
            if (parse_result == PARSELINE_FG) {
                add_job(pid, FG, cmdline);
            }
            if (parse_result == PARSELINE_BG) {
                add_job(pid, BG, cmdline);
            }
            // Unblock SIGCHLD
            sigprocmask(SIG_SETMASK, &mask_one, NULL);

            // Wait if FG
            if (parse_result == PARSELINE_BG) {
                sigprocmask(SIG_BLOCK, &mask_all, NULL);
                printf("[%d] (%d) %s\n", job_from_pid(pid), pid, cmdline);
                sigprocmask(SIG_SETMASK, &mask_prev, NULL);
            } else {
                wait_SIGCHLD();
            }
            // Unblock signals
            sigprocmask(SIG_SETMASK, &mask_prev, NULL);
        }
    } else {
        // Built-in commands
        sigset_t mask_all, mask_prev;
        int out_fd = STDOUT_FILENO;
        jid_t jid;
        pid_t pid;

        sigfillset(&mask_all);
        if (token.builtin == BUILTIN_QUIT) {
            exit(EXIT_SUCCESS);
        }

        if (token.builtin == BUILTIN_JOBS) {
            sigprocmask(SIG_SETMASK, &mask_all, &mask_prev);

            if (token.outfile) {
                if ((out_fd = open(token.outfile, O_WRONLY | O_TRUNC | O_CREAT,
                                   S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) <
                    0) {
                    perror(token.outfile);
                    strerror(errno);
                    sigprocmask(SIG_SETMASK, &mask_prev, NULL);
                    return;
                }
            }
            if (!list_jobs(out_fd)) {
                perror("List job failed");
                strerror(errno);
            }
            if (token.outfile) {
                close(out_fd);
            }
            sigprocmask(SIG_SETMASK, &mask_prev, NULL);
        }

        if (token.builtin == BUILTIN_FG || token.builtin == BUILTIN_BG) {
            if (!token.argv[1]) {
                if (token.builtin == BUILTIN_FG)
                    sio_printf("fg");
                else
                    sio_printf("bg");
                fflush(stdout);
                sio_printf(" command requires PID or %%jobid argument\n");
                return;
            }
            sigprocmask(SIG_SETMASK, &mask_all, &mask_prev);
            if (token.argv[1][0] == '%') {
                // JID
                jid = atoi(token.argv[1] + 1);
                if (!job_exists(jid)) {
                    printf("%s: No such job\n", token.argv[1]);
                    sigprocmask(SIG_SETMASK, &mask_prev, NULL);
                    return;
                }
            } else {
                // PID
                pid = atoi(token.argv[1]);
                jid = job_from_pid(pid);
                if (!jid) {
                    if (token.builtin == BUILTIN_BG)
                        sio_printf("bg");
                    else
                        sio_printf("fg");
                    fflush(stdout);
                    sio_printf(": argument must be a PID or %%jobid\n");
                    sigprocmask(SIG_SETMASK, &mask_prev, NULL);
                    return;
                }
            }
            sigprocmask(SIG_SETMASK, &mask_prev, NULL);

            if (token.builtin == BUILTIN_FG) {
                if (!to_FG(jid)) {
                    printf("Command Failed\n");
                    return;
                }
            } else {
                if (!to_BG(jid)) {
                    printf("Command Failed\n");
                    return;
                }
            }
        }
    }
    return;
}

/*****************
 * Signal handlers
 *****************/

/**
 * @brief Child process end signal handler
 *
 * When receive SIGCHLD, reap zombie process and set flag
 * to unblock main process if fg process exists.
 */
void sigchld_handler(int sig) {
    int olderrno = errno;
    sigset_t mask_all, mask_prev;
    pid_t pid;
    jid_t jid;
    int status;

    sigfillset(&mask_all);

    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {
        sigprocmask(SIG_BLOCK, &mask_all, &mask_prev);
        if (fg_job() > 0 && pid == job_get_pid(fg_job())) {
            flag = 1;
        }
        jid = job_from_pid(pid);
        if (WIFSTOPPED(status)) {
            job_set_state(jid, ST);
            sio_printf("Job [%d] (%d) stopped by signal %d\n", jid, pid,
                       WSTOPSIG(status));
        } else {
            if (WIFSIGNALED(status))
                sio_printf("Job [%d] (%d) terminated by signal %d\n", jid, pid,
                           WTERMSIG(status));
            delete_job(jid);
        }
        sigprocmask(SIG_SETMASK, &mask_prev, NULL);
    }

    errno = olderrno;
}

/**
 * @brief SIGINT handler
 *
 * When receive SIGINT, terminate foreground process
 * immediately and set flag to unblock main process
 */
void sigint_handler(int sig) {
    int olderrno = errno;
    sigset_t mask_all, mask_prev;
    pid_t pid = 0;
    jid_t jid = 0;

    sigfillset(&mask_all);

    sigprocmask(SIG_BLOCK, &mask_all, &mask_prev);
    jid = fg_job();
    if (jid)
        pid = job_get_pid(jid);

    if (pid) {
        kill(-pid, SIGINT);
    } else {
    }
    sigprocmask(SIG_SETMASK, &mask_prev, NULL);
    errno = olderrno;
    return;
}

/**
 * @brief SIGTSTP handler
 *
 * When receive SIGTSTP signal, stop foreground process/group
 * and unblock main process
 */
void sigtstp_handler(int sig) {
    int olderrno = errno;
    sigset_t mask_all, mask_prev;
    pid_t pid = 0;
    jid_t jid = 0;
    sigfillset(&mask_all);
    sigprocmask(SIG_BLOCK, &mask_all, &mask_prev);

    jid = fg_job();
    if (jid)
        pid = job_get_pid(jid);

    if (pid) {
        kill(-pid, SIGTSTP);
    } else {
    }

    sigprocmask(SIG_SETMASK, &mask_prev, NULL);
    errno = olderrno;
    return;
}

/**
 * @brief Block main process
 *
 * Block main process until further signals arrives.
 */
void wait_SIGCHLD(void) {
    sigset_t mask;
    sigemptyset(&mask);
    flag = 0;

    while (flag == 0) {
        sigsuspend(&mask);
    }
    return;
}

/**
 * @brief Send Stopped jobs and background jobs to foreground
 */
int to_FG(jid_t jid) {
    pid_t pid;
    sigset_t mask_all, mask_one, mask_prev;
    job_state state;
    sigfillset(&mask_all);
    sigaddset(&mask_one, SIGCHLD);

    sigprocmask(SIG_BLOCK, &mask_all, &mask_prev);

    pid = job_get_pid(jid);
    state = job_get_state(jid);
    switch (state) {
    case FG:
    case UNDEF:
        perror("JOB STATE INVALID");
        sigprocmask(SIG_SETMASK, &mask_prev, NULL);
        return 0;
    case BG:
    case ST:
    default:
        job_set_state(jid, FG);
        if (state == ST) {
            kill(-pid, SIGCONT);
        }
        sigprocmask(SIG_SETMASK, &mask_one, NULL);
        wait_SIGCHLD();
        break;
    }

    sigprocmask(SIG_SETMASK, &mask_prev, NULL);
    return 1;
}

/**
 * @brief Send Stopped jobs to background
 */
int to_BG(jid_t jid) {
    pid_t pid;
    sigset_t mask_all, mask_one, mask_prev;
    job_state state;
    sigfillset(&mask_all);
    sigaddset(&mask_one, SIGCHLD);

    sigprocmask(SIG_BLOCK, &mask_all, &mask_prev);

    pid = job_get_pid(jid);
    state = job_get_state(jid);
    switch (state) {
    case ST:
        job_set_state(jid, BG);
        kill(-pid, SIGCONT);
        sio_printf("[%d] (%d) %s \n", jid, pid, job_get_cmdline(jid));
        break;
    case BG:
        break;
    case UNDEF:
    case FG:
        perror("JOB STATE INVALID");
        sigprocmask(SIG_SETMASK, &mask_prev, NULL);
        return 0;
    default:
        break;
    }

    sigprocmask(SIG_SETMASK, &mask_prev, NULL);
    return 1;
}

/**
 * @brief Attempt to clean up global resources when the program exits.
 *
 * In particular, the job list must be freed at this time, since it may
 * contain leftover buffers from existing or even deleted jobs.
 */
void cleanup(void) {
    // Signals handlers need to be removed before destroying the joblist
    Signal(SIGINT, SIG_DFL);  // Handles Ctrl-C
    Signal(SIGTSTP, SIG_DFL); // Handles Ctrl-Z
    Signal(SIGCHLD, SIG_DFL); // Handles terminated or stopped child

    destroy_job_list();
}
