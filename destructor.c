#include "kshell.h"

void* _command_text(struct command_text* cmd){
    int i;
    if(!cmd) return NULL;
    if(cmd->in_file){
        free(cmd->in_file);
    }
    if(cmd->out_file){
        free(cmd->out_file);
    }
    if(cmd->command){
        free(cmd->command);
    }
    if(cmd->end_of_param > 0){
        i = 0;
        while(cmd->params[i]){
            free(cmd->params[i++]);
        }
        i = 0;
        while(cmd->argv[i]){
            free(cmd->argv[i++]);
        }
    }else{
        i = 0;
        while(cmd->params[i]){
            free(cmd->params[i++]);
        }
    }
    free(cmd->params);
    if(cmd->end_of_param > 0) free(cmd->argv);
    free(cmd);
    return NULL;
}

void* _process(struct process* pcs){
    if(!pcs) return NULL;
    free(pcs);
    return NULL;
}