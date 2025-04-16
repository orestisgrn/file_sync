#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include "worker.h"
#include "string.h"

int full_sync(char *source,char *target);
int add_file(char *source,char *target,char *file);
int modify_file(char *source,char *target,char *file);
int deleted_file(char *source,char *target,char *file);

String build_path(char *source,char *target);
int write_report(char *err);

int main(int argc,char **argv) {
    if (!strcmp(argv[3],"ALL")) {
        return full_sync(argv[1],argv[2]);
    }
    char op = argv[4][0] - '0';     // assumes operation codes are single digit numbers
    switch (op) {
        case ADDED:
            return add_file(argv[1],argv[2],argv[3]);
        case MODIFIED:
            return modify_file(argv[1],argv[2],argv[3]);
        case DELETED:
            return deleted_file(argv[1],argv[2],argv[3]);
    }
}

int full_sync(char *source,char *target) {
    DIR *dir_ptr;
    if ((dir_ptr=opendir(source))==NULL) {
        return write_report(strerror(errno));
    }
    struct dirent *direntp;
    while ((direntp=readdir(dir_ptr))!=NULL) {
        String file_path = build_path(source,direntp->d_name);
        if (file_path==NULL)
            return write_report(strerror(errno));
        write(STDOUT_FILENO,string_ptr(file_path),string_length(file_path)+1);
        string_free(file_path);
    }
    closedir(dir_ptr);
    return 0;
}

int add_file(char *source,char *target,char *file) {
    return 0;
}

int modify_file(char *source,char *target,char *file) {
    return 0;
}

int deleted_file(char *source,char *target,char *file) {
    return 0;
}

int write_report(char *err) {
    write(STDOUT_FILENO,err,strlen(err)+1);//
    return -1;
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