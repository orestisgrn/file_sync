#include <unistd.h>
#include <string.h>

int main(int argc,char **argv) {
    char s1[] = "I will handle ";
    write(STDOUT_FILENO,s1,sizeof(s1)-1);
    write(STDOUT_FILENO,argv[1],strlen(argv[1])+1);
    return 0;
}