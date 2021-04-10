#include "kshell.h"

int get_pid(int jid){
    int i;
    for(i=0;i<MAX_JOB;i++){
        if(pcs_list[i] && pcs_list[i]->jid == jid){
            return pcs_list[i]->pid;
        }
    }
    return 0;
}

int is_builtin(struct command_text* cmd){
    if(!strcmp(cmd->command,"quit")) return 1;
    if(!strcmp(cmd->command,"jobs")) return 1;
    if(!strcmp(cmd->command,"fg")) return 1;
    if(!strcmp(cmd->command,"bg")) return 1;
    if(!strcmp(cmd->command,"kill")) return 1;
    return 0;
}

int exec_builtin(struct command_text* cmd){
    //TODO... finish builtin functions
    char* p;
    int jid;
    pid_t pid;
    if(!strcmp(cmd->command,"quit")){
        // 0 means end loop
        return 0;
    }
    if(!strcmp(cmd->command,"jobs")){
        // 0 means end loop
        return _jobs();
    }
    if(!strcmp(cmd->command,"kill")){
        // 0 means end loop
        if(!cmd->params[1]) return 1;
        p = cmd->params[1];
        if(p[0] == '%'){
            jid = atoi(p+sizeof(char));
            pid = get_pid(jid);
            if(pid > 0)
                return _kill(pid);
            else
                return 1;
        }else{
            pid = (pid_t)atoi(p);
            return _kill(pid);
        }
    }
    if(!strcmp(cmd->command,"bg")){
        // 0 means end loop
        if(!cmd->params[1]) return 1;
        p = cmd->params[1];
        if(p[0] == '%'){
            jid = atoi(p+sizeof(char));
            pid = get_pid(jid);
            if(pid > 0)
                return _bg(pid);
            else
                return 1;
        }else{
            pid = (pid_t)atoi(p);
            return _bg(pid);
        }
    }
    if(!strcmp(cmd->command,"fg")){
        // 0 means end loop
        if(!cmd->params[1]) return 1;
        p = cmd->params[1];
        if(p[0] == '%'){
            jid = atoi(p+sizeof(char));
            pid = get_pid(jid);
            if(pid > 0)
                return _fg(pid);
            else
                return 1;
        }else{
            pid = (pid_t)atoi(p);
            return _fg(pid);
        }
    }
}

int _jobs(){
    int i,j;
    int new_jid;
    new_jid = 1;
    for(i=0;i<MAX_JOB;i++){
        if(pcs_list[i]){
            if(pcs_list[i]->flag == BACKGROUND){
                printf("[%d]<%d> Running ",new_jid,pcs_list[i]->pid);
                pcs_list[i]->jid = new_jid++;
            }
            if(pcs_list[i]->flag == STOPPED){
                printf("[%d]<%d> Stopped ",new_jid,pcs_list[i]->pid);
                pcs_list[i]->jid = new_jid++;
            }
            for(j=0;j<MAX_ARG;j++){
                if(!cmd_list[i]->params[j]) break;
                printf("%s ",cmd_list[i]->params[j]);
            }
            printf("\n");
        }
    }
    return 1;
}

int _bg(pid_t pid){
    int i;
    for(i=0;i<MAX_JOB;i++){
        if(pcs_list[i] && pcs_list[i]->flag == STOPPED && pcs_list[i]->pid == pid){
            kill(pcs_list[i]->pid,SIGCONT);
            SIGCONT_CALL = 1;
            pcs_list[i]->flag = BACKGROUND;
        }
    }
    return 1;
}

int _fg(pid_t pid){
    int i,pcs_stat;
    for(i=0;i<MAX_JOB;i++){
        if(pcs_list[i] && pcs_list[i]->pid == pid){
            pcs_list[i]->flag = FOREGROUND;
            kill(pcs_list[i]->pid,SIGCONT);
            SIGCONT_CALL = 1;
            FG_END = 0;
            SIGTSTP_CALL = 0;
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
                pcs_list[i] = _process(pcs_list[i]);
                cmd_list[i] = _command_text(cmd_list[i]);
            }else{
                SIGTSTP_CALL = 0;
            }
        }
    }
    return 1;
}

int _kill(pid_t pid){
    int i;
    kill(pid,SIGKILL);
    while(pid!=waitpid(pid,&i,WNOHANG)){}
    for(i=0;i<MAX_JOB;i++){
        if(pcs_list[i] && pcs_list[i]->pid == pid){
            pcs_list[i] = _process(pcs_list[i]);
            cmd_list[i] = _command_text(cmd_list[i]);
            pcs_list[i] = NULL;
            cmd_list[i] = NULL;
        }
    }
    return 1;
}

