#include "kshell.h"

int add_command(){
    int i;
    for(i=0;i<MAX_JOB;i++){
        if(!cmd_list[i] && !pcs_list[i]){
            return i;
        }
    }
    _kill(pcs_list[MAX_JOB-1]->pid);
    return MAX_JOB-1;
}

void initialize(){
    prompt = (char*)malloc(sizeof(char)*100);
    getcwd(prompt,100);

    cmd_list = (struct command_text**)malloc(sizeof(struct command_text*)*MAX_JOB);
    pcs_list = (struct process**)malloc(sizeof(struct process*)*MAX_JOB);

    buffer = (char*)malloc(sizeof(char)*MAX_LINE);
    SIGTSTP_CALL = 0;
    FG_END = 0;
    SIGCONT_CALL = 0;
}

void loop(){
    int RUNTIME;
    int cur;
    int pcs_stat;
    size_t size;
    pid_t pid;
    int in_fd;
    int out_fd;
    mode_t mode;

    struct command_text* t_cmd;

    RUNTIME = 1;
    pid = getpid();
    mode = S_IRWXU | S_IRWXG | S_IRWXO;
    in_fd = 0;
    out_fd = 0;
    while(RUNTIME){
        printf("%s> ",prompt);
        size = (size_t)fgets(buffer,MAX_LINE,stdin);
        if(!size){
            goto END_LOOP;
        }

        t_cmd = (struct command_text*)malloc(sizeof(struct command_text));
        parsing_command(buffer,t_cmd);
        if(is_builtin(t_cmd)){
            RUNTIME = exec_builtin(t_cmd); // Check for quit command
            _command_text(t_cmd);
            t_cmd = NULL;
            goto END_LOOP;
        }

        //start process
        cur = add_command();
        cmd_list[cur] = t_cmd;
        pcs_list[cur] = (struct process*)malloc(sizeof(struct process));
        pcs_list[cur]->jid = cur;
        pcs_list[cur]->flag = t_cmd->flag;
        pcs_list[cur]->pid = getpid();

        //execute process
        pid = fork();
        if(pid == 0){
            if(t_cmd->input == IN_REDIRECT){
                in_fd = open(t_cmd->in_file,O_RDONLY,mode);
                dup2(in_fd,STDIN_FILENO);
            }
            if(t_cmd->output == OUT_REDIRECT){
                out_fd = open(t_cmd->out_file, O_CREAT|O_WRONLY|O_TRUNC,mode);
                dup2(out_fd,STDOUT_FILENO);
            }
            if(access(t_cmd->command,F_OK) == 0){
                execv(t_cmd->command,t_cmd->argv);
            }else{
                execvp(t_cmd->command,t_cmd->argv);
            }
            if(t_cmd->input == IN_REDIRECT){
                close(in_fd);
            }
            if(t_cmd->input == OUT_REDIRECT){
                close(out_fd);
            }
            exit(END);
        }else{
            pcs_list[cur]->pid = pid;
            if(pcs_list[cur]->flag == FOREGROUND){
                //Clear up foreground process when finished/ hang up process when interrupted
                while(1){
                    if(SIGTSTP_CALL){
                        pcs_stat = STOP;
                        break;
                    }
                    if(FG_END){
                        FG_END = 0;
                        pcs_stat = END;
                        break;
                    }
                }
                if(pcs_stat == END){
                    pcs_list[cur] = _process(pcs_list[cur]);
                    cmd_list[cur] = _command_text(cmd_list[cur]);
                }else{
                    SIGTSTP_CALL = 0;
                }
            }
        }

        END_LOOP:
        memset(buffer,0,sizeof(char)*MAX_LINE);
    }
}

void clear(){
    int i;
    if(buffer) free(buffer);
    free(prompt);
    for(i=0;i<MAX_JOB;i++){
        if(!pcs_list[i] || !cmd_list[i]){
            continue;
        }
        _kill(pcs_list[i]->pid);
        _command_text(cmd_list[i]);
        _process(pcs_list[i]);
    }
    free(cmd_list);
    free(pcs_list);
}

void signal_handler(int SIGNAL){
    int status,i;
    pid_t pid;
    if(SIGNAL == SIGTSTP){
        //Revive casualtiy
        for(i=0;i<MAX_JOB;i++){
            if(pcs_list[i] && pcs_list[i]->flag == BACKGROUND){
                pcs_list[i]->flag = STOPPED;
                _bg(pcs_list[i]->pid);
            }
        }
        //Hang up foreground
        for(i=0;i<MAX_JOB;i++){
            if(pcs_list[i] && pcs_list[i]->flag == FOREGROUND){
                SIGTSTP_CALL = 1;
                pid = pcs_list[i]->pid;
                kill(pid,SIGTSTP);
                pcs_list[i]->flag = STOPPED;
                return;
            }
        }
    }

    if(SIGNAL == SIGCHLD){
        if(SIGCONT_CALL){
            SIGCONT_CALL = 0;
            return;
        }
        //Some process ended, reap it
        for(i=0;i<MAX_JOB;i++){
            if(pcs_list[i]){
                if(pcs_list[i]->flag == FOREGROUND || pcs_list[i]->flag == BACKGROUND) {
                    if (pcs_list[i]->flag == FOREGROUND) { FG_END = 1; usleep(100);}
                    waitpid(pcs_list[i]->pid, &status, WNOHANG);
                    return;
                }
            }
        }
    }
}


int main(int argc, char** argv){
    printf("Kay Shell Terminal Linux [Version Alpha 0.2]\n(p) 2021 Handsome Kay. All rights reserved.\n\n");
    initialize();
    signal(SIGTSTP,signal_handler);
    signal(SIGCHLD,signal_handler);
    loop();
    clear();
    return END;
}