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

int signal_fd;
int cur_workers = 0;
int worker_limit = 5;
sigset_t sigchld;

Sync_Info_Lookup sync_info_mem_store;
Queue worker_queue;

FILE *log_file;

String create_all_string(void);

char *fss_in  = FSS_IN;
char *fss_out = FSS_OUT;

int read_config(FILE *config_file, int inotify_fd);
int spawn_worker(struct work_rec *work_rec);
void handle_worker_term(struct sync_info_rec *rec, pid_t pid);
int process_command(String cmd);
int collect_workers(void);
int cleanup_workers(void);

int main(int argc,char **argv) {
    FILE *config_file=NULL;
    int fss_in_fd=-1,fss_out_fd=-1;
    unlink(fss_in);
    unlink(fss_out);
    if (mkfifo(fss_in,0660)==-1 || mkfifo(fss_out,0660)==-1) {
        CLEAN_AND_EXIT(perror("Fifos couldn't be created\n"),FIFO_ERR);
    }
    fss_in_fd   = open(fss_in,O_RDONLY | O_NONBLOCK);
    //fss_out_fd  = open(fss_out,O_WRONLY);
    if (fss_in_fd==-1) {
        CLEAN_AND_EXIT(perror("Fifo couldn't open\n"),FIFO_ERR);
    }
    sigemptyset(&sigchld);
    sigaddset(&sigchld,SIGCHLD); 
    sigprocmask(SIG_SETMASK,&sigchld,NULL);
    if ((signal_fd=signalfd(-1,&sigchld,SFD_NONBLOCK))==-1) {
        perror("Signal file descriptor initialization error\n");
        return SIGNALFD_ERR;
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
    if (logname==NULL) {
        perror("No logfile given\n");
        return ARGS_ERR;
    }
    if ((log_file=fopen(logname,"a"))==NULL) {
        perror("Logfile couldn't open\n");
        return FOPEN_ERR;
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
        if ((code=read_config(config_file,inotify_fd))!=0) {
            CLEAN_AND_EXIT(perror("Error while reading config file\n"),code);
        }
    }
    enum { INOTIFY_FD, FSS_IN_FD, SIGNAL_FD };
    struct pollfd waiting_fds[] = { {inotify_fd,POLLIN,0}, {fss_in_fd,POLLIN,0}, {signal_fd,POLLIN,0} };
    printf("Waiting for events here\n");
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
            if (read(fss_in_fd,&ch,sizeof(ch))==0) {
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
                    if (string_push(cmd,ch)==-1)
                        CLEAN_AND_EXIT(perror("Memory allocation error\n"),ALLOC_ERR);
                    if (ch=='\n')
                        break;
                    read(fss_in_fd,&ch,sizeof(ch));
                }
                process_command(cmd);
                string_free(cmd);
            }
            waiting_fds[FSS_IN_FD].revents=0;
        }
        // Continue for other cases
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

int read_config(FILE *config_file, int inotify_fd) {
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
        int watch_desc=inotify_add_watch(inotify_fd,string_ptr(source),IN_CREATE | IN_DELETE | IN_MODIFY);
        int insert_code;
        struct work_rec work_rec;
        work_rec.rec = sync_info_insert(sync_info_mem_store,source,target,watch_desc,&insert_code);
        //printf("%d %d\n",insert_code,watch_desc);
        if (insert_code==DUPL) {
            //printf("\nEntry %s detected twice. Duplicate entry omitted.\n\n",string_ptr(source));
            inotify_rm_watch(inotify_fd,watch_desc);    // Warning: may have inotify msgs not needed
            string_free(source);                        // Solution -> hash individually per diff key
            string_free(target);
            continue;
        }
        else if (insert_code==FAILED) {
            string_free(source);
            string_free(target);
            return ALLOC_ERR;
        }
        work_rec.filename = create_all_string();
        if (work_rec.filename==NULL)
            return ALLOC_ERR;
        int collect_code;
        if ((collect_code=collect_workers())!=0)
            return collect_code;
        int spawn_code;
        work_rec.op = FULL;
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
    while (read(signal_fd,&siginfo,sizeof(siginfo))!=-1) {
        int status;
        pid_t pid = wait(&status);
        struct work_rec *work_rec;
        cur_workers--;
        struct sync_info_rec *rec = sync_info_watchdesc_search(sync_info_mem_store,WEXITSTATUS(status));
        handle_worker_term(rec,pid);
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

int cleanup_workers(void) {
    int status;
    pid_t pid;
    while((pid=wait(&status))!=-1) {
        struct work_rec *work_rec;
        cur_workers--;
        struct sync_info_rec *rec = sync_info_watchdesc_search(sync_info_mem_store,WEXITSTATUS(status));
        handle_worker_term(rec,pid);
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

int spawn_worker(struct work_rec *work_rec) {
    if (cur_workers==worker_limit) {
        if (!queue_push(worker_queue,work_rec->rec,work_rec->filename,work_rec->op)) {
            perror("Couldn't push worker to queue\n");
            string_free(work_rec->filename);
            return ALLOC_ERR;
        }
    }
    else {
        cur_workers++;
        if (pipe(work_rec->rec->pipes)==-1) {
            perror("Pipe couldn't be created\n");
            return PIPE_ERR;
        }
        pid_t pid;
        if ((pid=fork())==-1) {             // Maybe queue no forked work recs
            perror("Worker process couldn't be spawned\n");
            return FORK_ERR;
        }
        else if (pid==0) {
            dup2(work_rec->rec->pipes[0],STDIN_FILENO);
            dup2(work_rec->rec->pipes[1],STDOUT_FILENO);
            write(STDOUT_FILENO,&work_rec->rec->watch_desc,sizeof(int));  // warning: change types if struct changes
            char op_str[2] = { work_rec->op+'0','\0' };
            execl("./worker","worker",string_ptr(work_rec->rec->source_dir),
                    string_ptr(work_rec->rec->target_dir),string_ptr(work_rec->filename),op_str,NULL);
            perror("Worker binary couldn't be executed\n");
            return EXEC_ERR;
        }
        string_free(work_rec->filename);
    }
    return 0;
}

void handle_worker_term(struct sync_info_rec *rec, pid_t pid) {
    rec->last_sync_time = time(NULL);       // Only full sync for now
    int op,file_num,success_num,buffer_count;
    read(rec->pipes[0],&op,sizeof(op));
    read(rec->pipes[0],&file_num,sizeof(file_num));
    read(rec->pipes[0],&success_num,sizeof(success_num));
    read(rec->pipes[0],&buffer_count,sizeof(buffer_count));
    char *result;
    char *copy_msg;
    int two_args=0;
    if (success_num==file_num) {
        result="SUCCESS";
        copy_msg="[%d files copied]\n";
    }
    else if (success_num==0) {
        result="ERROR";
        copy_msg="[%d skipped]\n";
    }
    else {
        result="PARTIAL";
        copy_msg="[%d files copied, %d skipped]\n";
        two_args=1;
    }
    char time_str[30];
    strftime(time_str,30,"%Y-%m-%d %H:%M:%S",localtime(&rec->last_sync_time));
    fprintf(log_file,"[%s] [%s] [%s] [%d] [%s]\n[%s] ",time_str,string_ptr(rec->source_dir),
                                                        string_ptr(rec->target_dir),pid,op_strings[op],
                                                        result);
    if (two_args)
        fprintf(log_file,copy_msg,success_num,file_num-success_num);
    else
        fprintf(log_file,copy_msg,file_num);
    char ch;
    for (int i=0;i<buffer_count;i++) {  //
        do {
            read(rec->pipes[0],&ch,sizeof(ch));
            putchar(ch);
        } while (ch!='\n');
    }
    // Maybe check for the pipe errors here
    close(rec->pipes[0]);
    close(rec->pipes[1]);
}

int process_command(String cmd) {           // Command ends in \n
    const char *cmd_ptr = string_ptr(cmd);
    String argv = string_create(15);
    if (argv==NULL) {
        return -1;
    }
    while (1) {
        while (!isspace(*cmd_ptr)) {
            if (string_push(argv,*cmd_ptr)==-1) {
                string_free(argv);
                return -1;
            }
            cmd_ptr++;
        }
        if (string_length(argv)!=0) {
            printf("%s\n",string_ptr(argv));
            string_free(argv);
            argv = string_create(15);
            if (argv==NULL) {
                return -1;
            }
        }
        if (*(++cmd_ptr)=='\0')
            break;
    }
    string_free(argv);
    return 0;
}

String create_all_string(void) {
    String all=string_create(3);
    if (all==NULL)
        return NULL;
    string_cpy(all,"ALL");
    return all;
}