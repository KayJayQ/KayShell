#include "kshell.h"

void parsing_command(char* buffer, struct command_text* cmd){
    char** params;
    char* p;
    int i;
    params = (char**)malloc(sizeof(char*)*MAX_ARG);
    for(i=0;i<MAX_ARG;i++){
        params[i] = (char*)malloc(sizeof(char)*MAX_LINE);
        memset(params[i],0,sizeof(char)*MAX_LINE);
    }

    p = strtok(buffer," \t\n");
    cmd->command = (char*)malloc(sizeof(char)*MAX_LINE);
    cmd->params = (char**)malloc(sizeof(char**)*MAX_ARG);
    memset(cmd->command,0,sizeof(char)*MAX_LINE);
    memset(cmd->params,0,sizeof(char)*MAX_LINE);
    strcpy(cmd->command,p);
    cmd->flag = FOREGROUND;
    cmd->input = STDIN_FILENO;
    cmd->output = STDOUT_FILENO;
    cmd->end_of_param = -1;
    i = 0;
    cmd->params[i] = (char*)malloc(sizeof(char)*MAX_LINE);
    strcpy(cmd->params[i++],cmd->command);
    while(1){
        p = strtok(NULL, " \t\n");
        if(!p) break;
        cmd->params[i] = (char*)malloc(sizeof(char)*MAX_LINE);
        strcpy(cmd->params[i],p);

        if(strlen(p) == 1 && ispunct(p[0])){
            if(cmd->end_of_param < 0) {cmd->end_of_param = i;}
            switch(p[0]){
                case '<':
                    cmd->input = IN_REDIRECT + i+1;
                    break;
                case '>':
                    cmd->output = OUT_REDIRECT + i+1;
                    break;
                case '&':
                    cmd->flag = BACKGROUND;
                    break;
                default:
                    cmd->end_of_param = -1;
                    break;
            }
        }

        i++;
    }
    cmd->in_file = NULL;
    cmd->out_file = NULL;

    if(cmd->input != STDIN_FILENO){
        cmd->in_file = (char*)malloc(sizeof(char)*MAX_LINE);
        strcpy(cmd->in_file, cmd->params[cmd->input-IN_REDIRECT]);
        cmd->input = IN_REDIRECT;
    }
    if(cmd->output != STDOUT_FILENO){
        cmd->out_file = (char*)malloc(sizeof(char)*MAX_LINE);
        strcpy(cmd->out_file, cmd->params[cmd->output-OUT_REDIRECT]);
        cmd->output = OUT_REDIRECT;
    }

    if(cmd->end_of_param > 0){
        cmd->argv = (char**)malloc(sizeof(char*)*cmd->end_of_param);
        memset(cmd->argv,0,sizeof(char*)*cmd->end_of_param);
        for(i=0;i<cmd->end_of_param;i++){
            cmd->argv[i] = (char*)malloc(sizeof(char)*strlen(cmd->params[i]));
            strcpy(cmd->argv[i],cmd->params[i]);
        }
    }else{
        cmd->argv = cmd->params;
    }
}
