#include <unistd.h>

int main(int argc,char **argv) {
    char s1[] = "I will handle the work for you";
    write(STDOUT_FILENO,s1,sizeof(s1));
    return 0;
}