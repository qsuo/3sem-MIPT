#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>

int error(const char* msg)
{
    fprintf(stderr, "%s\n", msg);
    exit(EXIT_FAILURE);
}

typedef struct msg_s
{
    int type;
    int data;
} msg_t;

long handle(int argc, char* argv[])
{
    if(argc != 2)
        error("Invalid arguments");

    const int base = 10;

    char* endptr = NULL;
    char* str = argv[1];
    long value = 0;

    errno = 0;
    value = strtol(str, &endptr, base);

    if(errno == ERANGE)
        error("Out of range");

    if((endptr == str) || (*endptr != '\0'))
        error("Not a number");

    return value;
}

int makeChilds(long n)
{
     
    int id = msgget(IPC_PRIVATE, IPC_CREAT | 0666);
    if(id == 0)
        error("Can't create msg queue");
        
    msg_t msg;
    
    int size = sizeof(msg) - sizeof(msg.type);

    msg.type = n + 1;
    msgsnd(id, &msg, size, 0);

       
    int i = 1;
    for(i = 1; i <= n; i++)
    {
        pid_t pid = fork();
        if(pid == 0)
        {
            if(msgrcv(id, &msg, size, i + 1, 0) == -1)
                error("Can't receive\n");

            
            printf("%d ", i);
            fflush(stdout);
            msg.type = i;
            
            if(msgsnd(id, &msg, size, 0) == -1)
                error("Can't send\n");

            return 0;
        }
    }

    msgrcv(id, &msg, size, 1, 0);

    printf("\n");

    msgctl(id, IPC_RMID, NULL);
    return id;
}

int main(int argc, char* argv[])
{
    long n = handle(argc, argv);


    makeChilds(n);

    return 0;
}
