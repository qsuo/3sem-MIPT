#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <math.h>
#include <sys/select.h>


#define BUFSIZE 128 * 1024

const long MB1 = 1024*1024;

int error(const char* msg)
{
    fprintf(stderr, "%s\n", msg);
    exit(EXIT_FAILURE);
}


long handle(int argc, char* argv[], int* fd)
{
    const char* usage = "Usage: ./prog [FILE] [NUMBER OF INTERMEDIARIES]";
    if(argc != 3)
        error(usage);

    *fd = open(argv[1], O_RDONLY);
    if(*fd == -1)
        error("No such file");


    const int base = 10;

    char* endptr = NULL;
    char* str = argv[2];
    long value = 0;

    errno = 0;
    value = strtol(str, &endptr, base);

    if(errno == ERANGE)
        error("Out of range in number");

    if((endptr == str) || (*endptr != '\0'))
        error(usage);

    return value;
}

typedef struct channel_s
{
    int rpipe;
    int wpipe;
    long n;
    
    char *buf;

    int maxfd;
    size_t current;

} channel_t;


enum
{
    R = 0,
    W,
};

enum mode
{
    PARENT,
    CHILD
};

const int CLOSED = -1;

typedef struct pipes_s
{
    int in[2];
    int out[2];
} pipes_t;


long min(long a, long b)
{
    if(a < b)
        return a;
    else
        return b;
}

long max(long a, long b)
{
    if(a > b)
        return a;
    else
        return b;
}

int server(long n, int fileFd)
{

    double dsize = 512 * pow(3, n);

    size_t size = (size_t) dsize;


    pipes_t* pipes = (pipes_t*) calloc(n, sizeof(pipes_t));
    if(pipes == NULL)
    {
        fprintf(stderr, "pipe err calloc\n");
        return 0;
    }

    int rfd = 0;
    int wfd = 0;

    long i;

    int pid = 0;

    for(i = 0; i < n; i++)
    {

        pipe(pipes[i].in);
        pipe(pipes[i].out);

       
        pid = fork();

        if(pid > 0)
            break;
       

        if(pid == 0)
        {

            close(pipes[i].in[W]);
            close(pipes[i].out[R]);

            if(i == 0)
            {
                rfd = fileFd;
                wfd = pipes[i].out[W];
              
                close(pipes[i].in[R]);
            }
            
            if(0 < i && i < n - 1)
            {
                rfd =  pipes[i].in[R];
                wfd = pipes[i].out[W];

            }

            if(i == n -1)
            {
                rfd =  pipes[i].in[R];
                wfd = STDOUT_FILENO;

                close(pipes[i].out[W]);
            }

        }
    }

    if(pid > 0)
    {

        channel_t* channel = (channel_t*) calloc(n - 1, sizeof(channel_t));
        if(channel == NULL)
        {
            fprintf(stderr, "connections err calloc\n");
            free(pipes);
            return 0;
        }

        
        int maxfd = 0;

        close(pipes[0].in[W]);


        for(i = 0; i < n - 1; i++)
        {

            size_t curSize = min(size, MB1);
            
            close(pipes[i].in[R]);
            close(pipes[i].out[W]);

            channel[i].rpipe = pipes[i].out[R];
            channel[i].wpipe = pipes[i + 1].in[W];

            /*maxfd = max(maxfd, rpipe);
            maxfd = max(maxfd, wpipe);*/

            /*fcntl(rpipe, F_SETFL, O_RDONLY | O_NONBLOCK);
            fcntl(wpipe, F_SETFL, O_WRONLY | O_NONBLOCK);*/


            //this->connection[i] = createConnection(curSize, rpipe, wpipe);

            size /= 3;
        }

        close(pipes[n - 1].out[W]);

        //this->maxfd = maxfd + 1;
        char buf[BUFSIZE];
        size_t nread = 1;
        while(nread > 0)
        {
            nread = read(channel[0].rpipe, buf, BUFSIZE);
            write(STDOUT_FILENO, buf, nread);
        }


        pipes = NULL;
    }


    free(pipes);
    

    /*if(pid > 0)
    {
        char buf[BUFSIZE];
        size_t nread = 1;
        while(nread > 0)
        {
            nread = read(channel[0].rpipe, buf, BUFSIZE);
            write(STDOUT_FILENO, buf, nread);
        }
    }*/

    if(pid == 0)
    {
        sleep(1);
        char buf[BUFSIZE];
        size_t nread = 1;
        while(nread > 0)
        {
            nread = read(rfd, buf, BUFSIZE);
            write(wfd, buf, nread);
        }

    }



    return 0;
}


int main(int argc, char* argv[])
{
    int fd = 0;

    long n = handle(argc, argv, &fd);

    server(n, fd);

    close(fd);
    return 0;
}


