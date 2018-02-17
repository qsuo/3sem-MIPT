#include <stdio.h>
#include <assert.h>
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

enum
{
    R = 0,
    W,
};

enum
{
    CHILD,
    PARENT
};

const int CLOSED = -1;


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

    if(value <= 1)
        error("Must be more than 1 intermediaies");

    return value;
}

typedef struct channel_s
{
    int rfd;
    int wfd;
    size_t size;
    char* buf;
    int current;
    char* start;
} channel_t;


channel_t* createChannel(int rfd, int wfd, size_t size)
{
    channel_t* this = (channel_t*) calloc(1, sizeof(channel_t));
    if(this == NULL)
        return NULL;

    this->size = size;
    this->rfd = rfd;
    this->wfd = wfd;
    this->current = 0;

    this->buf = (char*) calloc(size, sizeof(char));
    if(this->buf == NULL)
    {
        free(this);
        return NULL;
    }

    this->start = this->buf;
    return this;

}

int deleteChannel(channel_t* this)
{
    assert(this != NULL);

    this->size = 0;

    close(this->rfd);
    close(this->wfd);
    
    this->rfd = CLOSED;
    this->wfd = CLOSED;

    
    free(this->buf);
    this->buf = NULL;
    return 0;

}

typedef struct dealer_s
{
    int rfd;
    int wfd;
} dealer_t;


typedef struct server_s
{
    int mode;
    int fileFd;
    long n;

    int maxfd;

    dealer_t dealer;
    channel_t* channel;

} server_t;


server_t* createServer(long n, int fileFd)
{
    server_t* this = (server_t*) calloc(1, sizeof(server_t));
    if(this == NULL)
        return NULL;

    this->n = n;
    this->fileFd = fileFd;

    int success = 1;

    long i = 0;
    int maxfd = -1;

    this->channel = (channel_t*) calloc(n - 1, sizeof(channel_t));
    if(this->channel == NULL)
    {
        fprintf(stderr, "Channels error calloc\n");
        goto channelCallocError;
    }

    int in[2];
    int out[2];


    for(i = 0; i < n; i++)
    {
        int r1 = pipe(in);
        int r2 = pipe(out);

        if(r1 == -1 || r2 == -1)
        {
            fprintf(stderr,"Pipe error\n");
            goto pipeError;
        }


        pid_t pid = fork();
    
        if(pid == -1)
        {
            fprintf(stderr, "Fork error\n");
            goto forkError;
        }

        if(pid == 0)
        {
            long j;
            for(j = 0; j < i; j++)
            {
                close(this->channel[j].rfd);
                close(this->channel[j].wfd);
            }


            close(in[W]);
            close(out[R]);


            if(i == 0)
            {
                this->dealer.rfd = fileFd;
                this->dealer.wfd = out[W];
                
                close(in[R]);
            }

            if(0 < i && i < n - 1)
            {
                this->dealer.rfd = in[R];
                this->dealer.wfd = out[W];
            }

            if(i == n - 1)
            {
                this->dealer.rfd = in[R];
                this->dealer.wfd = STDOUT_FILENO;

                close(out[W]);
            }
        
            this->mode = CHILD;

            return this;
        }


        if(pid > 0)
        {
            
            this->mode = PARENT;
            
            close(in[R]);
            close(out[W]);

            maxfd = max(maxfd, out[R]);
            maxfd = max(maxfd, in[W]);

            fcntl(out[R], F_SETFL, O_RDONLY | O_NONBLOCK);
            fcntl(in[W], F_SETFL, O_WRONLY | O_NONBLOCK);


            if(i == 0)
            {
                this->channel[i].rfd = out[R];
                close(in[W]);
            }

            if(0 < i && i < n - 1)
            {
                this->channel[i].rfd = out[R];
                this->channel[i - 1].wfd = in[W];
            }

            if(i == n - 1)
            {
                this->channel[i - 1].wfd = in[W];
                close(out[R]);
            }

        }
    }

    this->maxfd = maxfd + 1;

    for(i = 0; i < n - 1; i++)
    {

        size_t size = (size_t) 512 * pow(3, n - i);

        size_t curSize = 0;

        if(size == 0)
            curSize = MB1;
        else
            curSize = min(size, MB1);
      

        this->channel[i].size = curSize;
        this->channel[i].buf = (char*) calloc(curSize, sizeof(char));
        this->channel[i].current = 0;

        this->channel[i].start = this->channel[i].buf;

        if(this->channel[i].buf == NULL)
        {
            fprintf(stderr, "Cant allocate for buffers\n");
            
            goto bufCallocError;
        }
    }

    return this;

    int k;
bufCallocError:

    for(k = 0; k < i; k++)
        deleteChannel(&this->channel[k]);

forkError:
pipeError:    
    
    free(this->channel);

channelCallocError:
    
    free(this);
    return NULL;
}

int deleteServer(server_t* this)
{
    if(this == NULL)
        return -1;
    
    if(this->mode == PARENT)
    {   
        long i;
        for(i = 0; i < this->n - 1; i++)
            deleteChannel(&this->channel[i]);

    }

    free(this->channel);
    free(this);

    return 0;
}


int startServer(server_t* server)
{

    int maxfd = server->maxfd;
    long n = server->n;
    long i;

    
    int passing = 1;

    fd_set rfds;
    fd_set wfds;

    while(passing)
    {
        FD_ZERO(&rfds);
        FD_ZERO(&wfds);
        
        passing = 0;
        
        
        for(i = 0; i < n - 1; i++)
        {
            if( server->channel[i].rfd != CLOSED && 
                server->channel[i].current == 0)
            {
                FD_SET(server->channel[i].rfd, &rfds);
                passing++;
            }

            if( server->channel[i].wfd != CLOSED &&
                server->channel[i].current != 0)
            {
                FD_SET(server->channel[i].wfd, &wfds);
                passing++;
            }
                        
        }

        if(passing == 0)
            break;

        int ret = select(maxfd, &rfds, &wfds, NULL, NULL);
        if(ret == -1)
        {
            perror(NULL);
            fprintf(stderr, "select err\n");
            return 0;
        }

        ssize_t nread = 0;  
        
        int k = 0;
        for(i = 0; i < n - 1; i++)
        {
            if(FD_ISSET(server->channel[i].rfd, &rfds))
            {
                nread = read(   server->channel[i].rfd, 
                                server->channel[i].buf,
                                server->channel[i].size);

                server->channel[i].current += nread;


                if(nread == 0)
                {
                    close(server->channel[i].rfd);
                    close(server->channel[i].wfd);
                    server->channel[i].wfd = CLOSED;
                    server->channel[i].rfd = CLOSED;
                }
            }

            if(FD_ISSET(server->channel[i].wfd, &wfds))
            {

                size_t nwrite = write(  server->channel[i].wfd,
                                        server->channel[i].start,
                                        server->channel[i].current);

                if(nwrite < server->channel[i].current)
                    server->channel[i].start += nwrite;
                else
                    server->channel[i].start = server->channel[i].buf;

                server->channel[i].current -= nwrite;
                
            }
        }


    }


    return 0;
}


int dealerAction(int rfd, int wfd)
{
    char buf[BUFSIZE];

    int nread = 1;

    while(nread > 0)
    {
        nread = read(rfd, buf, BUFSIZE);
        
        int nwrite = write(wfd, buf, nread);
    }

    return 0;
}

int dumpChannel(channel_t* channel)
{
    printf("rfd = %d, wfd = %d\n", channel->rfd, channel->wfd);
    return 0;
}

int main (int argc, char* argv[])
{
    int fd = 0;
    long n = handle(argc, argv, &fd);

    server_t* server = createServer(n, fd);

    if(server == NULL)
        return 0;
    
    if(server->mode == CHILD)
    {

        int rfd = server->dealer.rfd;
        int wfd = server->dealer.wfd;

        deleteServer(server);

        dealerAction(rfd, wfd);


    }

    else if(server->mode == PARENT)
    {
        startServer(server);
        deleteServer(server);
    }

    return 0;
}

