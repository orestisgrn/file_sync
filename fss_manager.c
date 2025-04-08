#include <stdio.h>
#include <stdlib.h>
#include "utils.h"

int main(int argc,char **argv) {
    int worker_limit = 5;
    char opt = '\0';
    char *logfile = NULL;
    char *config = NULL;
    while (*(++argv) != NULL) {
        if ((opt == 0) && ((*argv)[0] == '-')) {
            opt = (*argv)[1];
        }
        else {
            switch (opt) {
                case 'l':
                    logfile = *argv;
                    break;
                case 'c':
                    config = *argv;
                    break;
                case 'n':
                    char *wrong_char=NULL;
                    worker_limit = strtol(*argv,&wrong_char,10);
                    if (*wrong_char!='\0') {
                        perror("Worker limit must be int\n");   // Maybe change error text to usage
                        return ARGS_ERR;
                    }
                    break;
                case '\0':
                    perror("Argument without option\n");
                    return ARGS_ERR;
            }
            opt = 0;
        }
    }
    if (opt != '\0') {
        perror("Option without argument\n");
        return ARGS_ERR;
    }
    if (config != NULL) {
        return 0;    
    }
    return 0;
}