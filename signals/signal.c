#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <signal.h>


#define BUFSIZE 2048



int err(const char* msg)
{
    fprintf(stderr, "%s\n", msg);
    exit(EXIT_FAILURE);
}

int showBits(char c)
{
    int x;
    
    for(x = 0x80; x > 0; x >>= 1)
    {
        int n = c & x;

        if(n != 0)
            printf("1");
        else
            printf("0");

    }
    printf("\n");

    return 0;
}

int parseBits(char c, int* array)
{
    int x = 0x80;

    int i;
    for(i = 0; i < 8; i++)
    {
        int n = c & x;
        
        if(n != 0)
            array[i] = 1;
        else
            array[i] = 0;

        x >>= 1;
    }
    return 0;
}


int cpid = 0;


void proceed(int signo)
{}

#define addAction(name, handler)\
    struct sigaction name = {};\
    name.sa_handler = handler;\
    sigemptyset(&name.sa_mask);\
    name.sa_flags = 0


int child(const char* file, pid_t ppid)
{
    int fd = open(file, O_RDONLY);
    if(fd == -1)
        err("Can't open file");


    addAction(act_proceed, proceed);
    sigaction(SIGUSR1, &act_proceed, NULL);


    sigset_t waitMask;
    sigfillset(&waitMask);
    sigdelset(&waitMask, SIGUSR1);
    sigdelset(&waitMask, SIGALRM);
    sigdelset(&waitMask, SIGINT);


    char buf[BUFSIZE];

    ssize_t nread = 0;
    while((nread = read(fd, buf, BUFSIZE)) > 0)
    {
        int curByte;
        for(curByte = 0; curByte < nread; curByte++)
        {
            int bit[8];
            parseBits(buf[curByte], bit);
            int curBit;
            for(curBit = 0; curBit < 8; curBit++)
            {
                if(bit[curBit] == 0)
                    kill(ppid, SIGUSR1);
                else
                    kill(ppid, SIGUSR2);

                alarm(1);
                sigsuspend(&waitMask);
            }
        }

    }

    close(fd);

    return 0;
}


int glb_counter = 0x80;
char glb_this = 0;

int initGlobals()
{
    glb_this = 0;
    glb_counter = 0x80;
    return 0;
}

void bit0(int signo)
{
    glb_counter >>= 1;
}

void bit1(int signo)
{
    glb_this += glb_counter;
    glb_counter >>= 1;
}

void terminate(int signo)
{
    exit(EXIT_FAILURE);
}

int parent()
{


    addAction(act_terminate, terminate);
    sigaction(SIGCHLD, &act_terminate, NULL);

    addAction(act_bit0, bit0);
    sigaction(SIGUSR1, &act_bit0, NULL);

    addAction(act_bit1, bit1);
    sigaction(SIGUSR2, &act_bit1, NULL);

    sigset_t waitMask;
    sigfillset(&waitMask);
    sigdelset(&waitMask, SIGUSR1);
    sigdelset(&waitMask, SIGUSR2);
    sigdelset(&waitMask, SIGCHLD);
    sigdelset(&waitMask, SIGINT);


    while(1)
    {
        sigsuspend(&waitMask);

        if(glb_counter == 0)
        {
            write(STDOUT_FILENO, &glb_this, 1);
            initGlobals();
        }

        kill(cpid, SIGUSR1);
    }

    return 0;
}

int main(int argc, char* argv[])
{
    if(argc < 2)
        err("Should be an input file");

    pid_t ppid = getpid();


    sigset_t mainMask;
    sigemptyset(&mainMask);
    sigaddset(&mainMask, SIGUSR1);
    sigaddset(&mainMask, SIGUSR2);
    sigaddset(&mainMask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &mainMask, NULL);

    cpid = fork();


    if(cpid < 0)
        err("Can't make child");
    
    if(cpid == 0)
        child(argv[1], ppid);

    else
        parent();

    return 0;
}
