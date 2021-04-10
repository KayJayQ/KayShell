#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <ctype.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>

#define MAX_JOB 6 //max background jobs 5 background + 1 foreground
#define MAX_ARG 80 //max number of arguments provided
#define MAX_LINE 80 //max length of a line

#define END 1
#define STOP 2

#define FOREGROUND 0
#define BACKGROUND 1
#define STOPPED 2

#define IN_REDIRECT 4
#define OUT_REDIRECT 8

char* buffer;
char* prompt;
int SIGTSTP_CALL;
int FG_END;
int SIGCONT_CALL;
struct command_text;
struct process;

struct command_text** cmd_list;
struct process** pcs_list;

struct command_text{
    int flag;
    int input;
    int output;
    int end_of_param;
    char* in_file;
    char* out_file;
    char* command;
    char** params;
    char** argv;
};

struct process{
    int jid;
    pid_t pid;
    int flag;
};

//destructors
void* _command_text(struct command_text*);
void* _process(struct process*);

int is_builtin(struct command_text*);
int exec_builtin(struct command_text*);

int _kill(pid_t);
int _bg(pid_t);
int _fg(pid_t);
int _jobs();

void parsing_command(char*, struct command_text*);
void signal_handler(int SIGNAL);

void initialize(); //initialize
void loop(); //mainloop
void clear(); //clear up

