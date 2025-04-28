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
    if (opt != '\0') {
        perror("Option without argument\n");
        return ARGS_ERR;
    }
    int fss_in_fd,fss_out_fd;
    if ((fss_in_fd=open(fss_in,O_WRONLY | O_NONBLOCK))==-1) {
        printf("The manager isn't running\n");
        return -1;
    }
    while(1) {      // Main console loop            // Idea:redirect STDIN to fss_in (think a bit, huh?)
        int ch;
        String cmd = string_create(15);
        if (cmd==NULL) {
            perror("Memory allocation error");
            return ALLOC_ERR;
        }
        int wrote_char=0;
        printf("> ");
        do {
            ch = getchar();
            if (ch==EOF) {
                printf("EOF: terminating console...\n");
                string_free(cmd);
                close(fss_in_fd);
                return -2;
            }
            if (!isspace(ch))
                wrote_char = 1;
            if (string_push(cmd,ch)==-1) {
                string_free(cmd);
                perror("Memory allocation error");
                return ALLOC_ERR;
            }
        } while (ch!='\n');
        if (wrote_char) {
            write(fss_in_fd,string_ptr(cmd),string_length(cmd));
            if ((fss_out_fd=open(fss_out,O_RDONLY))==-1) {
                perror("fss_out couldn't open\n");
                return FIFO_ERR;
            }
            char return_code;
            read(fss_out_fd,&return_code,sizeof(return_code));
            if (return_code==INVALID) {
                printf("Invalid command: ");
                char ch;
                while (1) {
                    read(fss_out_fd,&ch,sizeof(ch));
                    if (ch=='\0')
                        break;
                    putchar(ch);
                }
                putchar('\n');
            }
            else if (return_code==SHUTDOWN) {
                printf("Shutting down...\n");
                string_free(cmd);
                close(fss_out_fd);
                break;
            }
            else {                                  // Continue from here
                read(fss_out_fd,&ch,sizeof(ch));
                if (ch==INVALID) {
                    printf("Source path doesn't exist.\n");
                }
                else if (ch==INVALID_TARGET) {
                    printf("Target path doesn't exist.\n");
                }
                else if (ch==NOT_ARCHIVED) {
                    printf("Source path is not archived.\n");
                }
                else if (ch==NOT_WATCHED) {
                    printf("Source path is not watched.\n");
                }
                else if (ch==ARCHIVED) {
                    printf("Source path is already archived.\n");
                }
            }
            close(fss_out_fd);
        }
        string_free(cmd);
    }
    close(fss_in_fd);
    return 0;
}