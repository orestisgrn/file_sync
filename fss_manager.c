#include <stdio.h>
#include <stdlib.h>
#include <sys/inotify.h>
#include <ctype.h>
#include "hashtable.h"
#include "string.h"
#include "utils.h"

int read_config(FILE *config_file, Hashtable sync_info_mem_store, int inotify_fd);

int main(int argc,char **argv) {
    int worker_limit = 5;
    int cur_workers = 0;
    char opt = '\0';
    char *logfile = NULL;
    char *config = NULL;
    FILE *config_file=NULL;
    Hashtable sync_info_mem_store=NULL;
    while (*(++argv) != NULL) {
        if ((opt == 0) && ((*argv)[0] == '-')) {
            opt = (*argv)[1];
        }
        else {
            char *wrong_char;
            switch (opt) {
                case 'l':
                    logfile = *argv;
                    break;
                case 'c':
                    config = *argv;
                    break;
                case 'n':
                    wrong_char=NULL;
                    worker_limit = strtol(*argv,&wrong_char,10);
                    if (*wrong_char!='\0') {
                        perror("Worker limit must be int\n");   // Maybe change error text to usage
                        return ARGS_ERR;
                    }
                    if (worker_limit < 1) {
                        perror("Worker limit must be a positive integer\n");
                        return ARGS_ERR;
                    }
                    break;
                case '\0':
                    perror("Argument without option\n");
                    return ARGS_ERR;
                default:
                    fprintf(stderr,"-%c is not an option\n",opt);
                    return ARGS_ERR;
            }
            opt = 0;
        }
    }
    if (opt != '\0') {
        perror("Option without argument\n");
        return ARGS_ERR;
    }
    int inotify_fd;
    if ((inotify_fd = inotify_init()) == -1) {
        perror("Inotify initialization error\n");
        return INOTIFY_ERR;
    }
    sync_info_mem_store = hashtable_create(200);
    if (config != NULL) {
        if ((config_file=fopen(config,"r"))==NULL) {
            perror("Config file couldn't open\n");
            hashtable_free(sync_info_mem_store);
            return FOPEN_ERR;
        }
        int code;
        if ((code=read_config(config_file,sync_info_mem_store,inotify_fd))!=0) {
            CLEAN_AND_EXIT(perror("Error while reading config file\n"),code);
        }
    }
    CLEAN_AND_EXIT( ,0);
    return 0;
}

int skip_white(FILE *file) {
    int ch;
    while(isspace(ch=fgetc(file)));
    return ch;
}

int read_config(FILE *config_file, Hashtable sync_info_mem_store, int inotify_fd) {
    int ch;
    String source,target;
    do {
        ch=skip_white(config_file);
        if (ch==EOF) {
            return 0;
        }
        if ((source=string_create(10))==NULL)
            return ALLOC_ERR;
        while (!isspace(ch)) {
            if (string_push(source,ch)==-1) {
                string_free(source);
                return ALLOC_ERR;
            }
            ch = fgetc(config_file);
            if (ch==EOF) {
                string_free(source);
                return 0;
            }
        }
        printf("%s\n",string_ptr(source));//
        ch = skip_white(config_file);
        if (ch==EOF) {
            string_free(source);
            return 0;
        }
        if ((target=string_create(10))==NULL) {
            string_free(source);
            return ALLOC_ERR;
        }
        while (!isspace(ch)) {
            if (string_push(target,ch)==-1) {
                string_free(source);
                string_free(target);
                return ALLOC_ERR;
            }
            ch = fgetc(config_file);
            if (ch==EOF) {
                break;
            }
        }
        printf("%s\n",string_ptr(target));//
        int insert_code = hashtable_insert(sync_info_mem_store,source,target);
        printf("%d\n",insert_code);
        if (insert_code==-1) {
            printf("\nEntry %s detected twice. Duplicate entry omitted.\n",string_ptr(source));
            string_free(source);
            string_free(target);
        }
        else if (insert_code==0) {
            string_free(source);
            string_free(target);
            return ALLOC_ERR;
        }
    } while (ch!=EOF);
    return 0;
}
