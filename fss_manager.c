#include <stdio.h>
#include <stdlib.h>
#include <sys/inotify.h>
#include <ctype.h>
#include <limits.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/signalfd.h>
#include <time.h>
#include <fcntl.h>
#include <poll.h>
#include <string.h>
#include "sync_info_lookup.h"
#include "string.h"
#include "utils.h"
#include "worker.h"
#include "queue.h"

#define INOTIFY_BUF (sizeof(struct inotify_event)+NAME_MAX+1)

int signal_fd = -1;
int cur_workers = 0;
int worker_limit = 5;
sigset_t sigchld;

Sync_Info_Lookup sync_info_mem_store;
Queue worker_queue;

struct worker_table_rec {
    struct sync_info_rec *rec;
    int pipes[2];
    pid_t pid;
};

struct worker_table_rec **worker_table;

FILE *log_file;

String create_all_string(void);

char *fss_in  = FSS_IN;
char *fss_out = FSS_OUT;

int fss_in_fd=-1;
int fss_out_fd=-1;

int inotify_fd=-1;

int pending_sync_wd = -1; 

int read_config(FILE *config_file);
int spawn_worker(struct work_rec *work_rec);
int handle_worker_term(struct worker_table_rec *worker);
int process_command(String cmd,char *cmd_code);
int collect_workers(void);
int cleanup_workers(void);

int main(int argc,char **argv) {
    FILE *config_file=NULL;
    unlink(fss_in);
    unlink(fss_out);
    if (mkfifo(fss_in,0660)==-1 || mkfifo(fss_out,0660)==-1) {
        CLEAN_AND_EXIT(perror("Fifos couldn't be created\n"),FIFO_ERR);
    }
    fss_in_fd = open(fss_in,O_RDONLY | O_NONBLOCK);
    //fss_out_fd  = open(fss_out,O_WRONLY);
    if (fss_in_fd==-1) {
        CLEAN_AND_EXIT(perror("Fifo couldn't open\n"),FIFO_ERR);
    }
    sigemptyset(&sigchld);
    sigaddset(&sigchld,SIGCHLD); 
    sigprocmask(SIG_SETMASK,&sigchld,NULL);
    if ((signal_fd=signalfd(-1,&sigchld,SFD_NONBLOCK))==-1) {
        CLEAN_AND_EXIT(perror("Signal file descriptor initialization error\n"),SIGNALFD_ERR);
    }
    char opt = '\0';
    char *logname = NULL;
    char *config = NULL;
    while (*(++argv) != NULL) {
        if ((opt == 0) && ((*argv)[0] == '-')) {
            opt = (*argv)[1];
        }
        else {
            char *wrong_char;
            switch (opt) {
                case 'l':
                    logname = *argv;
                    break;
                case 'c':
                    config = *argv;
                    break;
                case 'n':
                    wrong_char=NULL;
                    worker_limit = strtol(*argv,&wrong_char,10);
                    if (*wrong_char!='\0') {
                        CLEAN_AND_EXIT(perror("Worker limit must be int\n"),ARGS_ERR);
                    }   // Maybe change error text to usage
                    if (worker_limit < 1) {
                        CLEAN_AND_EXIT(perror("Worker limit must be a positive integer\n"),ARGS_ERR);
                    }
                    break;
                case '\0':
                    CLEAN_AND_EXIT(perror("Argument without option\n"),ARGS_ERR);
                default:
                    CLEAN_AND_EXIT(fprintf(stderr,"-%c is not an option\n",opt),ARGS_ERR);
            }
            opt = 0;
        }
    }
    if (logname==NULL) {
        CLEAN_AND_EXIT(perror("No logfile given\n"),ARGS_ERR);
    }
    if ((log_file=fopen(logname,"a"))==NULL) {
        CLEAN_AND_EXIT(perror("Logfile couldn't open\n"),FOPEN_ERR);
    }
    if (opt != '\0') {
        CLEAN_AND_EXIT(perror("Option without argument\n"),ARGS_ERR);
    }
    if ((inotify_fd = inotify_init()) == -1) {
        CLEAN_AND_EXIT(perror("Inotify initialization error\n"),INOTIFY_ERR);
    }
    sync_info_mem_store = sync_info_lookup_create(200);
    if (sync_info_mem_store==NULL) {
        CLEAN_AND_EXIT(perror("Memory allocation error\n"),ALLOC_ERR);
    }
    worker_table = calloc(worker_limit,sizeof(struct worker_table_rec*));
    if (worker_table==NULL) {
        CLEAN_AND_EXIT(perror("Memory allocation error\n"),ALLOC_ERR);
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
        if ((code=read_config(config_file))!=0) {
            CLEAN_AND_EXIT(perror("Error while reading config file\n"),code);
        }
    }
    enum { INOTIFY_FD, FSS_IN_FD, SIGNAL_FD };
    struct pollfd waiting_fds[] = { {inotify_fd,POLLIN,0}, {fss_in_fd,POLLIN,0}, {signal_fd,POLLIN,0} };
    printf("Waiting for events here\n");//
    while (poll(waiting_fds,sizeof(waiting_fds)/sizeof(waiting_fds[0]),-1)!=-1) {
        if (waiting_fds[SIGNAL_FD].revents!=0) {
            int collect_code;
            if ((collect_code=collect_workers())!=0) {
                CLEAN_AND_EXIT(perror("Worker couldn't spawn\n"),collect_code);
            }
            waiting_fds[SIGNAL_FD].revents=0;
            continue;
        }
        if (waiting_fds[FSS_IN_FD].revents!=0) {
            char ch;
            if (read(fss_in_fd,&ch,sizeof(ch))==0) {        // Nothing to read --->  fifo closed
                printf("Something happened\n");//
                close(fss_in_fd);
                fss_in_fd = open(fss_in,O_RDONLY | O_NONBLOCK);
                if (fss_in_fd==-1) {
                    CLEAN_AND_EXIT(perror("Fifo couldn't open\n"),FIFO_ERR);
                }
                break;//
            }
            else {
                String cmd = string_create(15);
                if (cmd==NULL) {
                    CLEAN_AND_EXIT(perror("Memory allocation error\n"),ALLOC_ERR);
                }
                while (1) {
                    if (string_push(cmd,ch)==-1) {
                        string_free(cmd);
                        CLEAN_AND_EXIT(perror("Memory allocation error\n"),ALLOC_ERR);
                    }
                    if (ch=='\n')
                        break;
                    read(fss_in_fd,&ch,sizeof(ch));
                }
                fss_out_fd=open(fss_out,O_WRONLY);
                if (fss_out_fd==-1) {
                    string_free(cmd);
                    CLEAN_AND_EXIT(perror("fss_out couldn't open\n"),FIFO_ERR);
                }
                char cmd_code;
                int err_code;
                if ((err_code=process_command(cmd,&cmd_code))!=0) {
                    close(fss_out_fd);
                    CLEAN_AND_EXIT({fprintf(stderr,"Error while executing command: %s\n",string_ptr(cmd));
                                    string_free(cmd);},err_code);
                }
                string_free(cmd);
                if (cmd_code!=SYNC)
                    close(fss_out_fd);
                if (cmd_code==SHUTDOWN)
                    break;
            }
            waiting_fds[FSS_IN_FD].revents=0;
        }
        if (waiting_fds[INOTIFY_FD].revents!=0) {
            static char event_buf[INOTIFY_BUF];         // WARNING!!! : May read MORE than one event
            int num_read = read(inotify_fd,event_buf,INOTIFY_BUF);
            for (char *p=event_buf;p<event_buf+num_read; ) {    // WARNING!!! : += not used here
                struct inotify_event *event = (struct inotify_event *) p;
                if (event->mask & IN_ISDIR) {                   // IN_INGORE event when rm-ing a watch
                    p += sizeof(struct inotify_event) + event->len;
                    continue;
                }
                struct sync_info_rec *rec = sync_info_watchdesc_search(sync_info_mem_store,event->wd);
                if (rec==NULL) {
                    printf("Why are you here?\n");//
                    p += sizeof(struct inotify_event) + event->len;
                    continue;
                }
                struct work_rec work_rec;
                work_rec.rec = rec;
                work_rec.from_queue = 0;
                work_rec.filename = string_create(15);
                if (work_rec.filename==NULL)
                    CLEAN_AND_EXIT(perror("Memory allocation error\n"),ALLOC_ERR);
                if (string_cpy(work_rec.filename,event->name)==-1) {
                    string_free(work_rec.filename);
                    CLEAN_AND_EXIT(perror("Memory allocation error\n"),ALLOC_ERR);
                }
                printf("mask = ");//
                if (event->mask & IN_CREATE) { printf("IN_CREATE "); work_rec.op = ADDED; }//
                if (event->mask & IN_DELETE) { printf("IN_DELETE "); work_rec.op = DELETED; }//
                if (event->mask & IN_MODIFY) { printf("IN_MODIFY "); work_rec.op = MODIFIED; }//
                printf("%ld\n",time(NULL));//
                printf("%s %s\n",string_ptr(rec->source_dir),event->name);//
                int spawn_code;
                // Maybe update with collect_workers here
                if ((spawn_code=spawn_worker(&work_rec))!=0)
                    CLEAN_AND_EXIT( ,spawn_code);
                p += sizeof(struct inotify_event) + event->len;
            }
            waiting_fds[INOTIFY_FD].revents=0;
        }
    }
    int cleanup_code;
    if ((cleanup_code=cleanup_workers())!=0)
        CLEAN_AND_EXIT( ,cleanup_code);
    //printf("%d\n",cur_workers);
    CLEAN_AND_EXIT( ,0);
}

int skip_white(FILE *file) {
    int ch;
    while(isspace(ch=fgetc(file)));
    return ch;
}

int read_config(FILE *config_file) {
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
            printf("\nSource path %s doesn't exist. Omitted\n\n",string_ptr(source));//?
            string_free(source);
            string_free(target);
            continue;
        }
        closedir(source_dir);
        char *real_source=realpath(string_ptr(source),NULL);
        string_free(source);
        if (real_source==NULL)
            return ALLOC_ERR;
        if ((source=string_create(10))==NULL)   // Maybe put length to create arg
            return ALLOC_ERR;
        if (string_cpy(source,real_source)==-1)
            return ALLOC_ERR;
        free(real_source);
        //printf("%s\n",string_ptr(source));
        //printf("%s\n",string_ptr(target));
        DIR *target_dir;
        if ((target_dir=opendir(string_ptr(target)))==NULL) {
            printf("\nTarget path %s doesn't exist. Omitted\n\n",string_ptr(target));//?
            string_free(source);
            string_free(target);
            continue;
        }
        closedir(target_dir);
        int insert_code;
        struct work_rec work_rec;
        work_rec.rec = sync_info_insert(sync_info_mem_store,source,target,&insert_code);
        printf("%d\n",insert_code);
        if (insert_code==DUPL) {
            printf("\nEntry %s detected twice. Duplicate entry omitted.\n\n",string_ptr(source));//
            string_free(source);
            string_free(target);
            continue;
        }
        else if (insert_code==FAILED) {
            string_free(source);
            string_free(target);
            return ALLOC_ERR;
        }
        int watch_desc=inotify_add_watch(inotify_fd,string_ptr(source),IN_CREATE | IN_DELETE | IN_MODIFY);
        printf("%d\n",watch_desc);
        work_rec.rec->watch_desc = watch_desc;
        if (sync_info_index_watchdesc(sync_info_mem_store,work_rec.rec,&insert_code)==NULL)
            return ALLOC_ERR;
        work_rec.filename = create_all_string();
        if (work_rec.filename==NULL)
            return ALLOC_ERR;
        int collect_code;
        if ((collect_code=collect_workers())!=0)
            return collect_code;
        int spawn_code;
        work_rec.op = FULL;
        work_rec.from_queue = 0;
        if ((spawn_code=spawn_worker(&work_rec))!=0)
            return spawn_code;
        char time_str[30];
        time_t t = time(NULL);
        strftime(time_str,30,"%Y-%m-%d %H:%M:%S",localtime(&t));
        printf("[%s] Added directory: %s -> %s\n",time_str,string_ptr(source),string_ptr(target));
        printf("[%s] Monitoring started for %s\n",time_str,string_ptr(source));// target is not normalized
        fprintf(log_file,"[%s] Added directory: %s -> %s\n",time_str,string_ptr(source),string_ptr(target));
        fprintf(log_file,"[%s] Monitoring started for %s\n",time_str,string_ptr(source));
    } while (ch!=EOF);
    return 0;
}

int collect_workers(void) {
    static struct signalfd_siginfo siginfo;
    int count=0;//
    int status;
    pid_t pid;
    while ((pid=waitpid(-1,&status,WNOHANG))>0) {
        read(signal_fd,&siginfo,sizeof(siginfo));
        struct work_rec *work_rec;
        int i;
        for (i=0;worker_table[i]->pid!=pid;i++);
        if (handle_worker_term(worker_table[i])==-1)
            return ALLOC_ERR;
        cur_workers--;
        free(worker_table[i]);
        worker_table[i] = worker_table[cur_workers];
        worker_table[cur_workers] = NULL;
        if ((work_rec=queue_pop(worker_queue))!=NULL) {
            int spawn_code;
            if ((spawn_code=spawn_worker(work_rec))!=0) {
                string_free(work_rec->filename);
                free(work_rec);
                return spawn_code;
            }
            free(work_rec);
        }
        count++;//
    }
    printf("%d\n",count);//
    return 0;
}

int cleanup_workers(void) {
    int status;
    pid_t pid;
    while((pid=wait(&status))!=-1) {
        struct work_rec *work_rec;
        int i;
        for (i=0;worker_table[i]->pid!=pid;i++);
        if (handle_worker_term(worker_table[i])==-1)
            return ALLOC_ERR;
        cur_workers--;
        free(worker_table[i]);
        worker_table[i] = worker_table[cur_workers];
        worker_table[cur_workers] = NULL;
        if ((work_rec=queue_pop(worker_queue))!=NULL) {
            int spawn_code;
            if ((spawn_code=spawn_worker(work_rec))!=0) {
                string_free(work_rec->filename);
                free(work_rec);
                return spawn_code;
            }
            free(work_rec);
        }
    }
    return 0;
}

int spawn_worker(struct work_rec *work_rec) {       // WARNING: spawn_worker is called both before
    if (!work_rec->from_queue)                      // and after a worker enters the queue
        work_rec->rec->worker_num++;
    if (cur_workers==worker_limit) {
        if (!queue_push(worker_queue,work_rec->rec,work_rec->filename,work_rec->op)) {
            perror("Couldn't push worker to queue\n");
            string_free(work_rec->filename);
            return ALLOC_ERR;
        }
    }
    else {
        worker_table[cur_workers] = malloc(sizeof(struct worker_table_rec));
        if (worker_table[cur_workers]==NULL) {
            return ALLOC_ERR;
        }
        cur_workers++;
        if (pipe(worker_table[cur_workers-1]->pipes)==-1) {
            perror("Pipe couldn't be created\n");
            return PIPE_ERR;
        }
        pid_t pid;
        if ((pid=fork())==-1) {             // Maybe queue no forked work recs
            perror("Worker process couldn't be spawned\n");
            return FORK_ERR;
        }
        else if (pid==0) {
            dup2(worker_table[cur_workers-1]->pipes[0],STDIN_FILENO);//
            dup2(worker_table[cur_workers-1]->pipes[1],STDOUT_FILENO);
            char op_str[2] = { work_rec->op+'0','\0' };
            execl("./worker","worker",string_ptr(work_rec->rec->source_dir),
                    string_ptr(work_rec->rec->target_dir),string_ptr(work_rec->filename),op_str,NULL);
            perror("Worker binary couldn't be executed\n");
            return EXEC_ERR;
        }
        worker_table[cur_workers-1]->pid = pid;
        worker_table[cur_workers-1]->rec = work_rec->rec;
        string_free(work_rec->filename);
    }
    return 0;
}

int handle_worker_term(struct worker_table_rec *worker) {
    struct sync_info_rec *rec = worker->rec;
    rec->worker_num--;
    rec->last_sync_time = time(NULL);
    int op,file_num,success_num,buffer_count;
    read(worker->pipes[0],&op,sizeof(op));
    read(worker->pipes[0],&file_num,sizeof(file_num));             // -1 means error
    read(worker->pipes[0],&success_num,sizeof(success_num));
    read(worker->pipes[0],&buffer_count,sizeof(buffer_count));
    char time_str[30];
    strftime(time_str,30,"%Y-%m-%d %H:%M:%S",localtime(&rec->last_sync_time));
    char *result;
    char *copy_msg;
    int two_args=0;
    fprintf(log_file,"[%s] [%s] [%s] [%d] [%s]\n",time_str,string_ptr(rec->source_dir),
                                                string_ptr(rec->target_dir),worker->pid,op_strings[op]);
    String error_str=NULL;
    if (success_num==file_num) {
        result="SUCCESS";
        if (op==FULL)
            copy_msg="[%d files copied]\n";
        else
            copy_msg="[File: %s]\n";
    }
    else if (success_num==0) {
        result="ERROR";
        if (op==FULL) {
            copy_msg="[%d skipped]\n";
        }
        else {
            copy_msg="[File: %s - %s]\n";
            if ((error_str=string_create(10))==NULL)
                return -1;
            char ch;
            while (1) {
                read(worker->pipes[0],&ch,sizeof(ch));
                if (ch=='\n')
                    break;
                if (string_push(error_str,ch)==-1) {
                    string_free(error_str);
                    return -1;
                }
            }
            two_args=1;
        }
    }
    else {
        result="PARTIAL";
        copy_msg="[%d files copied, %d skipped]\n";
        two_args=1;
    }
    fprintf(log_file,"[%s] ",result);
    if (op==FULL) {
        if (two_args)
            fprintf(log_file,copy_msg,success_num,file_num-success_num);
        else
            fprintf(log_file,copy_msg,file_num);
    }
    else {
        String filename=string_create(10);
        if (filename==NULL) {
            string_free(error_str);
            return -1;
        }
        char ch;
        while (1) {
            read(worker->pipes[0],&ch,sizeof(ch));
            if (ch=='\0')
                break;
            if (string_push(filename,ch)==-1) {
                string_free(error_str);
                string_free(filename);
                return -1;
            }
        }
        if (two_args)
            fprintf(log_file,copy_msg,string_ptr(filename),string_ptr(error_str));
        else
            fprintf(log_file,copy_msg,string_ptr(filename));
        string_free(error_str);
        string_free(filename);
    }
    // We don't check for error messages if op == FULL
    // Must also update rec
    close(worker->pipes[0]);
    close(worker->pipes[1]);
    if (pending_sync_wd == rec->watch_desc) {
        pending_sync_wd = -1;
        char sync = SYNC;
        write(fss_out_fd,&sync,sizeof(sync));
        printf("[%s] Sync completed %s -> %s Errors:%d\n",time_str,string_ptr(rec->source_dir),
                                                        string_ptr(rec->target_dir),file_num-success_num);
        fprintf(log_file,"[%s] Sync completed %s -> %s Errors:%d\n",time_str,string_ptr(rec->source_dir),
                                                        string_ptr(rec->target_dir),file_num-success_num);
        close(fss_out_fd);
    }
    return 0;
}

int handle_cmd(String argv);

int process_command(String cmd,char *cmd_code) {           // Command ends in \n
    *cmd_code = NO_COMMAND;
    const char *cmd_ptr = string_ptr(cmd);
    String argv = string_create(15);
    struct sync_info_rec *rec;
    char *real_source;
    int argc=0;
    if (argv==NULL) {
        return ALLOC_ERR;
    }
    while (1) {
        while (!isspace(*cmd_ptr)) {
            if (string_push(argv,*cmd_ptr)==-1) {
                string_free(argv);
                return ALLOC_ERR;
            }
            cmd_ptr++;
        }
        if (string_length(argv)!=0) {
            if (argc==0) {
                *cmd_code=handle_cmd(argv);
                if (*cmd_code==SHUTDOWN || *cmd_code==NO_COMMAND) {
                    string_free(argv);
                    return 0;
                }
                write(fss_out_fd,cmd_code,sizeof(*cmd_code));
                argc++;
            }
            else if (argc==1) {
                DIR *source_dir;
                if ((source_dir=opendir(string_ptr(argv)))==NULL) {
                    printf("\nSource path %s doesn't exist. Omitted\n\n",string_ptr(argv));//?
                    *cmd_code = INVALID_SOURCE;         // Think about only writing this to fss_out,
                    write(fss_out_fd,cmd_code,sizeof(*cmd_code));  // not return in *cmd_code
                    string_free(argv);
                    return 0;
                }
                closedir(source_dir);
                real_source=realpath(string_ptr(argv),NULL);
                if (real_source==NULL) {
                    string_free(argv);
                    return ALLOC_ERR;
                }
                if (*cmd_code==CANCEL) {
                    rec = sync_info_path_search(sync_info_mem_store,real_source);
                    if (rec==NULL) {
                        *cmd_code = NOT_ARCHIVED;
                        write(fss_out_fd,cmd_code,sizeof(*cmd_code));
                        string_free(argv);
                        free(real_source);
                        return 0;
                    }
                    if (rec->watch_desc==-1) {
                        *cmd_code = NOT_WATCHED;
                        write(fss_out_fd,cmd_code,sizeof(*cmd_code));
                        string_free(argv);
                        free(real_source);
                        return 0;
                    }
                    printf("%d\n",inotify_rm_watch(inotify_fd,rec->watch_desc));
                    sync_info_watchdesc_delete(sync_info_mem_store,rec->watch_desc);
                    rec->watch_desc=-1;
                    char time_str[30];
                    time_t t = time(NULL);
                    strftime(time_str,30,"%Y-%m-%d %H:%M:%S",localtime(&t));
                    printf("[%s] Monitoring stopped for %s\n",time_str,string_ptr(rec->source_dir));
                    fprintf(log_file,"[%s] Monitoring stopped for %s\n",time_str,string_ptr(rec->source_dir));
                    write(fss_out_fd,cmd_code,sizeof(*cmd_code));
                    argc++;
                    free(real_source);
                    break;
                }
                else if (*cmd_code==STATUS) {
                    rec = sync_info_path_search(sync_info_mem_store,real_source);
                    if (rec==NULL) {
                        *cmd_code = NOT_ARCHIVED;
                        write(fss_out_fd,cmd_code,sizeof(*cmd_code));
                        string_free(argv);
                        free(real_source);
                        return 0;
                    }
                    char time_str[30];
                    if (rec->last_sync_time==-1)
                        strcpy(time_str,"N/A");
                    else
                        strftime(time_str,30,"%Y-%m-%d %H:%M:%S",localtime(&rec->last_sync_time));
                    time_t t = time(NULL);
                    char cur_time_str[30];
                    strftime(cur_time_str,30,"%Y-%m-%d %H:%M:%S",localtime(&t));
                    printf("[%s] Status requested for %s\n",cur_time_str,string_ptr(rec->source_dir));
                    printf("Directory: %s\n",string_ptr(rec->source_dir));
                    printf("Target: %s\n",string_ptr(rec->target_dir));
                    printf("Last Sync: %s\n",time_str);
                    printf("Errors: %d\n",rec->error_count);
                    printf("Status: %s\n",rec->watch_desc!=-1 ? "Active" : "Inactive");
                    printf("Worker num: %d\n",rec->worker_num);//
                    write(fss_out_fd,cmd_code,sizeof(*cmd_code));       // Also send to console
                    argc++;
                    free(real_source);
                    break;
                }
                else if (*cmd_code==SYNC) {
                    rec = sync_info_path_search(sync_info_mem_store,real_source);
                    free(real_source);
                    if (rec==NULL) {
                        *cmd_code = NOT_ARCHIVED;
                        write(fss_out_fd,cmd_code,sizeof(*cmd_code));
                        string_free(argv);
                        return 0;
                    }
                    if (rec->watch_desc==-1) {
                        rec->watch_desc = inotify_add_watch(inotify_fd,string_ptr(rec->source_dir),IN_CREATE | IN_DELETE | IN_MODIFY);
                        int insert_code;
                        if (sync_info_index_watchdesc(sync_info_mem_store,rec,&insert_code)==NULL) {
                            string_free(argv);
                            return ALLOC_ERR;
                        }
                    }
                    pending_sync_wd = rec->watch_desc;
                    time_t t = time(NULL);
                    char cur_time_str[30];
                    strftime(cur_time_str,30,"%Y-%m-%d %H:%M:%S",localtime(&t));
                    if (rec->worker_num==0) {
                        struct work_rec work_rec;
                        work_rec.rec = rec;
                        work_rec.filename = create_all_string();
                        if (work_rec.filename==NULL) {
                            string_free(argv);
                            return ALLOC_ERR;
                        }
                        work_rec.op = FULL;
                        work_rec.from_queue = 0;
                        int spawn_code;
                        if ((spawn_code=spawn_worker(&work_rec))!=0) {
                            string_free(argv);
                            return spawn_code;
                        }
                        printf("[%s] Syncing directory: %s -> %s\n",cur_time_str,string_ptr(rec->source_dir),
                                                                                string_ptr(rec->target_dir));
                        fprintf(log_file,"[%s] Syncing directory: %s -> %s\n",cur_time_str,
                                                                        string_ptr(rec->source_dir),
                                                                        string_ptr(rec->target_dir));
                    }
                    else {
                        printf("[%s] Sync already in progress %s\n",cur_time_str,string_ptr(rec->source_dir));
                    }
                    // Think about the case where another worker finishes first instead of the new full sync
                    argc++;
                    break;
                }
                else if (*cmd_code==ADD) {
                    argc++;
                    rec = sync_info_path_search(sync_info_mem_store,real_source);
                    if (rec!=NULL) {
                        time_t t = time(NULL);
                        char cur_time_str[30];
                        strftime(cur_time_str,30,"%Y-%m-%d %H:%M:%S",localtime(&t));
                        printf("[%s] Already in queue: %s\n",cur_time_str,real_source);
                        *cmd_code = ARCHIVED;
                        write(fss_out_fd,cmd_code,sizeof(*cmd_code));
                        free(real_source);
                        break;
                    }
                }
            }
            else {
                DIR *target_dir;
                if ((target_dir=opendir(string_ptr(argv)))==NULL) {
                    printf("\nTarget path %s doesn't exist. Omitted\n\n",string_ptr(argv));//?
                    *cmd_code = INVALID_TARGET;
                    write(fss_out_fd,cmd_code,sizeof(*cmd_code));
                    string_free(argv);
                    free(real_source);
                    return 0;
                }
                closedir(target_dir);
                int insert_code;                    // Maybe make this section a function
                struct work_rec work_rec;
                String source = string_create(15);
                if (source==NULL) {
                    string_free(argv);
                    free(real_source);
                    return ALLOC_ERR;
                }
                if (string_cpy(source,real_source)==-1) {
                    string_free(argv);
                    string_free(source);
                    free(real_source);
                    return ALLOC_ERR;
                }
                free(real_source);
                work_rec.rec = sync_info_insert(sync_info_mem_store,source,argv,&insert_code);
                if (insert_code==FAILED) {
                    string_free(source);
                    string_free(argv);
                    return ALLOC_ERR;
                }
                int watch_desc=inotify_add_watch(inotify_fd,string_ptr(source),IN_CREATE | IN_DELETE | IN_MODIFY);
                printf("%d\n",watch_desc);
                work_rec.rec->watch_desc = watch_desc;
                if (sync_info_index_watchdesc(sync_info_mem_store,work_rec.rec,&insert_code)==NULL)
                    return ALLOC_ERR;
                work_rec.filename = create_all_string();
                if (work_rec.filename==NULL)
                    return ALLOC_ERR;
                int collect_code;
                if ((collect_code=collect_workers())!=0)
                    return collect_code;
                int spawn_code;
                work_rec.op = FULL;
                work_rec.from_queue = 0;
                if ((spawn_code=spawn_worker(&work_rec))!=0)
                    return spawn_code;
                char time_str[30];
                time_t t = time(NULL);
                strftime(time_str,30,"%Y-%m-%d %H:%M:%S",localtime(&t));
                printf("[%s] Added directory: %s -> %s\n",time_str,string_ptr(source),string_ptr(argv));
                printf("[%s] Monitoring started for %s\n",time_str,string_ptr(source));// target is not normalized
                fprintf(log_file,"[%s] Added directory: %s -> %s\n",time_str,string_ptr(source),string_ptr(argv));
                fprintf(log_file,"[%s] Monitoring started for %s\n",time_str,string_ptr(source));
                write(fss_out_fd,cmd_code,sizeof(*cmd_code));
                return 0;
            }
            string_free(argv);
            argv = string_create(15);
            if (argv==NULL) {
                return ALLOC_ERR;
            }
        }
        if (*(++cmd_ptr)=='\0')
            break;
    }
    string_free(argv);
    if (argc==1) {
        *cmd_code = INVALID_SOURCE;
        write(fss_out_fd,cmd_code,sizeof(*cmd_code));
    }
    if (argc==2 && *cmd_code==ADD) {
        *cmd_code = INVALID_TARGET;
        write(fss_out_fd,cmd_code,sizeof(*cmd_code));
        free(real_source);
    }
    return 0;
}

int handle_cmd(String argv) {
    char ch;
    if (!strcmp(string_ptr(argv),"shutdown")) {
        ch = SHUTDOWN;
        write(fss_out_fd,&ch,sizeof(ch));
        return SHUTDOWN;
    }
    else if (!strcmp(string_ptr(argv),"add")) {
        return ADD;
    }
    else if (!strcmp(string_ptr(argv),"cancel")) {
        return CANCEL;
    }
    else if (!strcmp(string_ptr(argv),"status")) {
        return STATUS;
    }
    else if (!strcmp(string_ptr(argv),"sync")) {
        return SYNC;
    }
    else {
        ch = NO_COMMAND;
        printf("%s\n",string_ptr(argv));
        write(fss_out_fd,&ch,sizeof(ch));
        write(fss_out_fd,string_ptr(argv),string_length(argv)+1);
        return ch;
    }
}

String create_all_string(void) {
    String all=string_create(3);
    if (all==NULL)
        return NULL;
    string_cpy(all,"ALL");
    return all;
}