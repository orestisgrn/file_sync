#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <fcntl.h>
#include "worker.h"
#include "string.h"

#define BUFFSIZE 100

void full_sync(char *source,char *target);
void add_file(char *source,char *target,char *file);        // warning: write report doesn't write the file name in the end
void modify_file(char *source,char *target,char *file);
void deleted_file(char *source,char *target,char *file);

String build_path(char *source,char *target);
void write_report(int op, char **err, int buffer_count, int file_num, int success_num);
void store_to_buffer(int err,char ***buffer, int *buffer_count, int *buffer_size);
int copy_file(String input, String output);

int main(int argc,char **argv) {
    int watch_desc;
    read(STDIN_FILENO,&watch_desc,sizeof(int));  // warning: change type if struct rec changes
    if (!strcmp(argv[3],"ALL")) {
        full_sync(argv[1],argv[2]);
        return watch_desc;
    }
    char op = argv[4][0] - '0';     // assumes operation codes are single digit numbers
    switch (op) {
        case ADDED:
            add_file(argv[1],argv[2],argv[3]);
            break;
        case MODIFIED:
            modify_file(argv[1],argv[2],argv[3]);
            break;
        case DELETED:
            deleted_file(argv[1],argv[2],argv[3]);
            break;
    }
    return watch_desc;
}

void full_sync(char *source,char *target) {
    DIR *dir_ptr;
    int buffer_size=50;
    char **buffer = malloc(buffer_size*sizeof(char*));
    int buffer_count=0;
    int file_num=0;
    int success_num=0;
    if (buffer==NULL) {
        char *err = strerror(errno);
        write_report(FULL,&err,1,-1,0);         // -1 on file_num means error (to not think zero files, zero copy)
        return;
    }
    if ((dir_ptr=opendir(source))==NULL) {
        char *err = strerror(errno);
        write_report(FULL,&err,1,-1,0);
        free(buffer);
        return;
    }
    struct dirent *direntp;
    errno = 0;
    while ((direntp=readdir(dir_ptr))!=NULL) {
        file_num++;
        String file_path = build_path(source,direntp->d_name);
        if (file_path==NULL) {
            store_to_buffer(errno,&buffer,&buffer_count,&buffer_size);
            continue;
        }
        String new_file_path = build_path(target,direntp->d_name);
        if (new_file_path==NULL) {
            store_to_buffer(errno,&buffer,&buffer_count,&buffer_size);
            string_free(file_path);
            continue;
        }
        int cp_code=copy_file(file_path,new_file_path);
        string_free(new_file_path);
        string_free(file_path);
        if (cp_code==-1) {
            file_num--;
            continue;
        }
        if (cp_code!=0) {
            store_to_buffer(cp_code,&buffer,&buffer_count,&buffer_size);
            continue;
        }
        success_num++;
        errno = 0;
    }
    if (errno != 0) {
        store_to_buffer(errno,&buffer,&buffer_count,&buffer_size);
    }
    write_report(FULL,buffer,buffer_count,file_num,success_num);
    free(buffer);
    closedir(dir_ptr);
}

void add_file(char *source,char *target,char *file) {
    String new_file_path = build_path(target,file);
    if (new_file_path==NULL) {
        char *err = strerror(errno);
        write_report(ADDED,&err,1,1,0);
        return;
    }
    if (open(string_ptr(new_file_path),O_WRONLY | O_CREAT | O_TRUNC,0644)==-1) {
        char *err = strerror(errno);
        write_report(ADDED,&err,1,1,0);
    }
    else {
        write_report(ADDED,NULL,0,1,1);
    }
    write(STDOUT_FILENO,file,strlen(file)+1);
}

void modify_file(char *source,char *target,char *file) {

}

void deleted_file(char *source,char *target,char *file) {

}

void write_report(int op, char **err, int buffer_count, int file_num, int success_num) {
    write(STDOUT_FILENO,&op,sizeof(op));
    write(STDOUT_FILENO,&file_num,sizeof(file_num));
    write(STDOUT_FILENO,&success_num,sizeof(success_num));
    write(STDOUT_FILENO,&buffer_count,sizeof(buffer_count));
    for (int i=0;i<buffer_count;i++) {
        write(STDOUT_FILENO,err[i],strlen(err[i]));
        write(STDOUT_FILENO,"\n",1);
    }
}

String build_path(char *source,char *target) {
    String path = string_create(10);
    if (path==NULL)
        return NULL;
    if (string_cpy(path,source)==-1) {
        string_free(path);
        return NULL;
    }
    if (string_push(path,'/')==-1) {
        string_free(path);
        return NULL;
    }
    if (string_cpy(path,target)==-1) {
        string_free(path);
        return NULL;
    }
    return path;
}

int copy_file(String input, String output) {        // Edge case: files with the name in target dir
    int infile,outfile;                             // already exist, but have no writing rights.
    int error;
    ssize_t nread ;
    char buffer[BUFFSIZE];
    if ((infile=open(string_ptr(input),O_RDONLY))==-1) {
        return errno;
    }
    struct stat in_stat;
    if (fstat(infile,&in_stat)==-1) {
        error = errno;
        close(infile);
        return error;
    }
    if ((in_stat.st_mode & S_IFMT) != S_IFREG) {
        close(infile);
        return -1;
    }
    if ((outfile=open(string_ptr(output),O_WRONLY | O_CREAT | O_TRUNC,0644))==-1) {
        error = errno;
        close(infile);
        return error;
    }
    while ((nread=read(infile,buffer,BUFFSIZE))>0) {
        if (write(outfile,buffer,nread)<nread) {
            error = errno;
            close(infile);
            close(outfile);
            return error;
        }
    }
    if (nread == -1) {
        error = errno;
        close(infile);
        close(outfile);
        return error;
    }
    close(infile);
    close(outfile);
    return 0;
}

void store_to_buffer(int err, char ***buffer, int *buffer_count, int *buffer_size) {
    if (*buffer_count==*buffer_size) {
        char **temp_buf = realloc(buffer,2*(*buffer_size)*sizeof(char*));
        if (temp_buf==NULL) {
            return;
        }
        *buffer = temp_buf;
        *buffer_size*=2;
    }
    (*buffer)[*buffer_count]=strerror(err);
    (*buffer_count)++;
    return;
}