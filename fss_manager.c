#include <stdio.h>
#include <stdlib.h>
#include <sys/inotify.h>
#include <ctype.h>
#include <limits.h>
#include <errno.h>
#include <sys/types.h>
#include <dirent.h>
#include "sync_info_lookup.h"
#include "string.h"
#include "utils.h"
#include "worker.h"
#include "queue.h"

int cur_workers = 0;
int worker_limit = 5;

int read_config(FILE *config_file, Sync_Info_Lookup sync_info_mem_store, int inotify_fd);

int main(int argc,char **argv) {
    char opt = '\0';
    char *logfile = NULL;
    char *config = NULL;
    FILE *config_file=NULL;
    Sync_Info_Lookup sync_info_mem_store=NULL;
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
    sync_info_mem_store = sync_info_lookup_create(200);
    if (config != NULL) {
        if ((config_file=fopen(config,"r"))==NULL) {
            perror("Config file couldn't open\n");
            sync_info_lookup_free(sync_info_mem_store);
            return FOPEN_ERR;
        }
        int code;
        if ((code=read_config(config_file,sync_info_mem_store,inotify_fd))!=0) {
            CLEAN_AND_EXIT(perror("Error while reading config file\n"),code);
        }
    }
    struct sync_info_rec *rec=sync_info_path_search(sync_info_mem_store,"/home/orestisgr123/Έγγραφα/excel_courses");
    printf("\n%d\n",rec->watch_desc);
    rec = sync_info_watchdesc_search(sync_info_mem_store,rec->watch_desc);
    printf("\n%s\n",string_ptr(rec->source_dir));
    CLEAN_AND_EXIT( ,0);
    return 0;
}

int skip_white(FILE *file) {
    int ch;
    while(isspace(ch=fgetc(file)));
    return ch;
}

int read_config(FILE *config_file, Sync_Info_Lookup sync_info_mem_store, int inotify_fd) {
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
        DIR *source_dir;
        if ((source_dir=opendir(string_ptr(source)))==NULL) {
            printf("\nSource path %s doesn't exist. Omitted\n\n",string_ptr(source));
            string_free(source);
            string_free(target);
            continue;
        }
        closedir(source_dir);
        char *real_source=realpath(string_ptr(source),NULL);
        string_free(source);
        if ((source=string_create(10))==NULL)   // Maybe put length to create arg
            return ALLOC_ERR;
        if (string_cpy(source,real_source)==-1)
            return ALLOC_ERR;
        free(real_source);
        printf("%s\n",string_ptr(source));//
        printf("%s\n",string_ptr(target));//
        DIR *target_dir;
        if ((target_dir=opendir(string_ptr(target)))==NULL) {
            printf("\nTarget path %s doesn't exist. Omitted\n\n",string_ptr(target));
            string_free(source);
            string_free(target);
            continue;
        }
        closedir(target_dir);
        int watch_desc=inotify_add_watch(inotify_fd,string_ptr(source),IN_CREATE | IN_DELETE | IN_MODIFY);
        int insert_code;
        struct sync_info_rec *rec = sync_info_insert(sync_info_mem_store,source,target,watch_desc,&insert_code);
        printf("%d %d\n",insert_code,watch_desc);
        if (insert_code==DUPL) {
            printf("\nEntry %s detected twice. Duplicate entry omitted.\n\n",string_ptr(source));
            inotify_rm_watch(inotify_fd,watch_desc);
            string_free(source);
            string_free(target);
        }
        else if (insert_code==FAILED) {
            inotify_rm_watch(inotify_fd,watch_desc);
            string_free(source);
            string_free(target);
            return ALLOC_ERR;
        }
        if (cur_workers==worker_limit) {
            continue;
        }
        else {
            continue;
        }
    } while (ch!=EOF);
    return 0;
}