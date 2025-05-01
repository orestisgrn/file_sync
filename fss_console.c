#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include "utils.h"
#include "string.h"

char *fss_in  = FSS_IN;
char *fss_out = FSS_OUT;

int fss_in_fd,fss_out_fd;

FILE *log_file;

void read_response(void);

int main(int argc, char **argv) {
    char opt='\0';
    char *logname=NULL;
    while (*(++argv) != NULL) {
        if ((opt == 0) && ((*argv)[0] == '-')) {
            opt = (*argv)[1];
        }
        else {
            switch (opt) {
                case 'l':
                    logname = *argv;
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
        fclose(log_file);
        return ARGS_ERR;
    }
    if ((fss_in_fd=open(fss_in,O_WRONLY | O_NONBLOCK))==-1) {
        printf("The manager isn't running\n");
        fclose(log_file);
        return -1;
    }
    while(1) {      // Main console loop            // Idea:redirect STDIN to fss_in (think a bit, huh?)
        int ch;
        String cmd = string_create(15);
        if (cmd==NULL) {
            perror("Memory allocation error");
            fclose(log_file);
            return ALLOC_ERR;
        }
        int wrote_char=0;
        printf("> ");
        do {
            ch = getchar();
            if (ch==EOF) {
                string_free(cmd);
                close(fss_in_fd);
                if ((fss_out_fd=open(fss_out,O_RDONLY))==-1) {
                    perror("fss_out couldn't open\n");
                    close(fss_in_fd);
                    fclose(log_file);
                    return FIFO_ERR;
                }
                putchar('\n');
                read_response();
                close(fss_out_fd);
                close(fss_in_fd);
                fclose(log_file);
                return -2;
            }
            if (!isspace(ch))
                wrote_char = 1;
            if (string_push(cmd,ch)==-1) {
                string_free(cmd);
                perror("Memory allocation error");
                close(fss_in_fd);
                fclose(log_file);
                return ALLOC_ERR;
            }
        } while (ch!='\n');
        if (wrote_char) {
            write(fss_in_fd,string_ptr(cmd),string_length(cmd));
            char return_code;
            if ((fss_out_fd=open(fss_out,O_RDONLY))==-1) {
                perror("fss_out couldn't open\n");
                fclose(log_file);
                return FIFO_ERR;
            }
            read(fss_out_fd,&return_code,sizeof(return_code));
            if (return_code==NO_COMMAND) {
                char buff[30];
                int chars_read;
                while ((chars_read=read(fss_out_fd,buff,sizeof(buff)))>0) {
                    fwrite(buff,sizeof(buff[0]),chars_read,stdout);
                    fwrite(buff,sizeof(buff[0]),chars_read,log_file);
                }
            }
            else if (return_code==SHUTDOWN) {
                read_response();
                close(fss_out_fd);
                string_free(cmd);
                break;
            }
            else {                                  // Continue from here
                char ch;
                read(fss_out_fd,&ch,sizeof(ch));
                if (ch==INVALID_SOURCE || ch==NOT_MONITORED || 
                    ch==NOT_ARCHIVED || ch==ARCHIVED || ch==INVALID_TARGET) {
                    char buff[30];
                    int chars_read;
                    while ((chars_read=read(fss_out_fd,buff,sizeof(buff)))>0)
                        fwrite(buff,sizeof(buff[0]),chars_read,stdout);
                }
                else {
                    read_response();
                }
            }
        }
        string_free(cmd);
    }
    fclose(log_file);
    close(fss_in_fd);
    return 0;
}

void read_response(void) {
    char ch;
    while (1) {
        read(fss_out_fd,&ch,sizeof(ch));
        putc(ch,log_file);
        if (ch=='\n')
            break;
    }
    char buff[30];
    int chars_read;
    while ((chars_read=read(fss_out_fd,buff,sizeof(buff)))>0) {
        fwrite(buff,sizeof(buff[0]),chars_read,stdout);
        fwrite(buff,sizeof(buff[0]),chars_read,log_file);
    }
}