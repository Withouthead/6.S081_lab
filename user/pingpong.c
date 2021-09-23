#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
// #include <stdio.h>
// #include <unistd.h>
// #include <stdlib.h>
int main(void)
{
    int pipefds1[2], pipefds2[2];
    int pid;
    // char pip1writemessage[20] = "ping";
    // char pip2writemessage[20] = "pong";
    char readmessage[20];
    pipe(pipefds1);
    pipe(pipefds2);
    // if((pipe(pipefds1) < 0))
    // {
    //     fprintf(2, "pip errroe\n");
    //     exit(1);
    // }
    // if((pipe(pipefds2) < 0))
    // {
        
    //     fprintf(2, "pip errroe\n");
    //     exit(1);
    // }
    pid = fork();
    if(pid < 0)
    {
        fprintf(2, "fork error\n");
        exit(1);
    }
    if(pid != 0)
    {
        close(pipefds1[0]);
        close(pipefds2[1]);
        
        write(pipefds1[1], "ping", 4);
        close(pipefds1[1]);
        read(pipefds2[0], readmessage, 4);
        printf("%d: received %s\n", getpid(), readmessage);
    }
    else
    {
        close(pipefds1[1]);
        close(pipefds2[0]);
        
        read(pipefds1[0], readmessage, 4);
        printf("%d: received %s\n", getpid(), readmessage);
        write(pipefds2[1], "pong", 4);
        close(pipefds2[1]);
        
    }
    exit(0);
}