#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// #include <stdio.h>
// #include <unistd.h>
// #include <stdlib.h>
// #include <sys/wait.h>
#define READMODE 0
#define WRITEMODE 1

int main()
{
    // int primers_array[40];
    // for(int i = 2; i <= 35; i++)
    //     primers_array[i] = i;
    
    int p[2];
    pipe(p);
    int read_pipe = p[READMODE];
    int write_pipe = p[WRITEMODE];
    close(0);
    int pppid = 0;
    if( (pppid = fork()) == 0)
    {
        while(1)
        {
            pipe(p);
            close(write_pipe);
            
            int base_num;
            if(read(read_pipe, &base_num, sizeof(int)) <= 0)
            {   
                // scanf("%d", &ttt);
                // printf("flage: %d\n", ttt);
                close(p[READMODE]);
                close(p[WRITEMODE]);
                close(read_pipe);
                exit(0);
            }
            printf("prime %d\n", base_num);
            int pid = fork();
            if(pid > 0)
            {   
                close(p[READMODE]);
                int x;
                while(read(read_pipe, &x, sizeof(int)))
                {
                    if(x % base_num != 0)
                    {
                        // printf("write %d to %d\n", x, pid);
                        write(p[WRITEMODE], &x, sizeof(int));
                    }
                }
                close(p[WRITEMODE]);
                close(read_pipe);
                wait((int*)0);
                
                
                exit(0);
            }
            else
            {
                read_pipe = p[READMODE];
                write_pipe = p[WRITEMODE];
            }
        }
    }
    else
    {
        close(p[READMODE]);
        for(int i = 2; i <= 35; i++)
        {
            write(p[WRITEMODE], &i, sizeof(int));
        }
        close(p[WRITEMODE]);
        
        wait((int*)0);
        exit(0);
    }
    exit(0);
}