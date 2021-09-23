#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"
int main(int argc, char *argv[])
{   
    char *cmd = argv[1];
    char *args[MAXARG];
    int arg_num = 0;
    for(int i = 1; i < argc; i++)
    {
        args[arg_num++] = argv[i];
    }
    int input_size = 0;
    char line_string[1024];
    char *one_arg;
    while((input_size = read(0, line_string, 1024)) > 0)
    {
        if(fork() == 0)
        {
            one_arg = (char *)malloc(sizeof line_string);
            int index = 0;
            for(int i = 0; i < input_size; i++)
            {
                if(line_string[i] == ' ' || line_string[i] == '\n')
                {
                    one_arg[index] = 0;
                    args[arg_num++] = one_arg;
                    one_arg = (char *)malloc(sizeof line_string);
                    index = 0;
                }
                else
                {
                    one_arg[index++] = line_string[i];
                }
            }
            args[arg_num] = 0;
            exec(cmd, args);
        }
        else
        {
            for(int i = 0; i < arg_num; i++)
                free(args[i]);
            wait((int *)0);
        }
    }
    exit(0);
}