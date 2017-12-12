#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>

#define BUFSIZE 1024
#define SIZENAME 10
#define WAIT 500


#define SLEEPTIME 200000

const char* FIFO =  "main.fifo";


int err(const char* msg)
{
    fprintf(stderr, "%s\n", msg);
    exit(EXIT_FAILURE);
}

int deleteNonBlock(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    flags &= ~O_NONBLOCK;
    fcntl(fd, F_SETFL, flags);

    return 0;
}

int sender(const char* file)
{
    int fd = open(file, O_RDONLY);
    if(fd == -1)
    {
        fprintf(stderr, "No such file\n");
        goto exit;
    }

    mkfifo(FIFO, 0666);
    int fifo = open(FIFO, O_RDONLY);

    pid_t receiverPid =  0;
    int ret = read(fifo, &receiverPid, sizeof(receiverPid));
    if(ret == 0)
    {
        fprintf(stderr, "Another sender is active\n");
        goto fd;
    }
    
    char receiverName[SIZENAME] = {};
    sprintf(receiverName, "%d.fifo", receiverPid);
    mkfifo(receiverName, 0666);

    int receiverFifo = open(receiverName, O_WRONLY | O_NONBLOCK);


    deleteNonBlock(receiverFifo);

    write(receiverFifo, "I", 1);

    char buf[BUFSIZE] = {};
    ssize_t count = 0;
    while((count = read(fd, buf, BUFSIZE)) > 0)
    {
        
        int written = write(receiverFifo, buf, count);
        if(written != count)
        {
            fprintf(stderr, "Write error\n");
            goto write;
        }
    }


write:
    close(receiverFifo);

    close(fifo);
    unlink(FIFO);
    
fd:
    close(fd);

exit:

    return 0;
}

int receiver()
{
    
    int myPid = getpid();
    char myName[SIZENAME] = {};
    sprintf(myName, "%d.fifo", myPid);
    mkfifo(myName, 0666);
    int myFifo = open(myName, O_RDONLY | O_NONBLOCK);

    
    mkfifo(FIFO, 0666);
    int fifo = open(FIFO, O_WRONLY);
    write(fifo, &myPid, sizeof(myPid));
    
    
    
    usleep(SLEEPTIME);
    
    deleteNonBlock(myFifo); 

    char buf[BUFSIZE] = {};


    
    if(read(myFifo, buf, 1) == 0)
    {
        fprintf(stderr, "Sender terminated or is busy\n");
        goto exit;
    }

    ssize_t count = 0;
    while((count = read(myFifo, buf, BUFSIZE)) > 0)
    {

        int written = write(STDOUT_FILENO, buf, count);
        if(written != count)
        {
            fprintf(stderr, "err\n");
            goto exit;
        }
    }

exit:
    close(fifo);
    close(myFifo);
    unlink(myName);

    return 0;
}


int main(int argc, char* argv[])
{
    
    if(argc == 1)
        receiver();

    else if(argc == 2)
        sender(argv[1]);

    else
        printf("Invalid arguments\n");

    return 0;
}



