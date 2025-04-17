#include <stdio.h>
#include <stdlib.h>
#include <sys/inotify.h>
#include <ctype.h>
#include <limits.h>
#include <errno.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include "sync_info_lookup.h"
#include "string.h"
#include "utils.h"
#include "worker.h"
#include "queue.h"

int cur_workers = 0;
int worker_limit = 5;

int read_config(FILE *config_file, Sync_Info_Lookup sync_info_mem_store, int inotify_fd);
void collect_worker(int signo);

int main(int argc,char **argv) {
    sigset_t mask;
    sigemptyset(&mask);
    static struct sigaction act;
    act.sa_handler = collect_worker;
    sigaction(SIGCHLD,&act,NULL);
    char opt = '\0';
    char *logfile = NULL;
    char *config = NULL;
    FILE *config_file=NULL;
    Sync_Info_Lookup sync_info_mem_store=NULL;
    Queue worker_queue=NULL;
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
    if (sync_info_mem_store==NULL) {
        perror("Memory allocation error\n");
        return ALLOC_ERR;
    }
    worker_queue = queue_create();
    if (worker_queue==NULL) {
        CLEAN_AND_EXIT(perror("Memory allocation error\n"),ALLOC_ERR);
    }
    if (config != NULL) {
        if ((config_file=fopen(config,"r"))==NULL) {
            CLEAN_AND_EXIT(perror("Config file couldn't open\n"),FOPEN_ERR);
        }
        int code;
        if ((code=read_config(config_file,sync_info_mem_store,inotify_fd))!=0) {
            CLEAN_AND_EXIT(perror("Error while reading config file\n"),code);
        }
    }
    int status;
    while (wait(&status)!=-1);
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
            continue;
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
            cur_workers++;
            if (pipe(rec->pipes)==-1) {
                perror("Pipe couldn't be created\n");
                return PIPE_ERR;
            }
            pid_t pid;
            if ((pid=fork())==-1) {
                perror("Worker process couldn't be spawned\n");
                return FORK_ERR;
            }
            else if (pid==0) {
                close(rec->pipes[0]);
                dup2(rec->pipes[1],STDOUT_FILENO);
                char op[2] = { FULL+'0','\0' };
                execl("./worker","worker",string_ptr(source),string_ptr(target),"ALL",op,NULL);
                perror("Worker binary couldn't be executed\n");
                return EXEC_ERR;
            }
            close(rec->pipes[1]);
        }
    } while (ch!=EOF);
    return 0;
}

void collect_worker(int signo) {
    //char text[] = "I got the child\n";
    //write(STDOUT_FILENO,text,sizeof(text));
}