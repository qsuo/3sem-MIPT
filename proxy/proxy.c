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

#define CBUFSIZE 128 * 1024

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


typedef struct connection_s
{
    int fdIn[2];
    int fdOut[2];
    int rpipe; 
    int wpipe;
    size_t size;
    char* buf;
} connection_t;

typedef struct channel_s
{
    int readFd;
    int writeFd;
    char buf[CBUFSIZE];
} channel_t;

/*typedef struct server_s
{
    int who;
    int inpipe[2];
    int outpipe[2];
    long n;
    connection_t** connection;
    channel_t channel;
} server_t;*/


connection_t* createConnection(size_t size, int rpipe, int wpipe)
{
    connection_t* this= (connection_t*) calloc(1, sizeof(connection_t));
    if(this == NULL)
        return NULL;

    this->size = size;
    this->rpipe = rpipe;
    this->wpipe = wpipe;

    this->buf = (char*) calloc(size, sizeof(char));
    if(this->buf == NULL)
        return NULL;

    return this;
}

int deleteConnection(connection_t* this)
{
    this->size = 0;
    this->rpipe = 0;
    this->wpipe = 0;
    free(this->buf);
    this->buf = NULL;
    free(this);
    this = NULL;
    return 0;
}


int dumpConnection(connection_t* this)
{
    printf("-------------\n");
    printf  ("size = %lu\n"
            "rpipe = %d\n"
            "wpipe = %d\n"
            "buf = %p\n",
            this->size, this->rpipe, this->wpipe, this->buf);
    printf("-------------\n");
    return 0;
}

int initChannel(channel_t* this, int readFd, int writeFd)
{
    this->readFd;
    this->writeFd;

    return 0;
}

enum
{
    R = 0,
    W
};

int deleteServerConnection(server_t* this, long n)
{
    if(this == NULL)
        return -1;

    long i;
    for(i = 0; i < n; i++)
    {
        free(this->connection[i]->buf);
        this->connection[i]->buf = NULL;
        free(this->connection[i]);
        this->connection[i] = NULL;
    }

    free(this->connection);
    this->connection = NULL;

}


int deleteServer(server_t* this)
{
    if (this == NULL)
        return -1;


    //if(this->connection == NULL)
     //   goto dserver;//just for not copypast
    
    long i;
    for(i = 0; i < this->n; i++)
    {
        /*if(this->connection[i] == NULL)
            continue;*/

        this->connection[i]->size = 0;
        this->connection[i]->rpipe = -1;
        this->connection[i]->wpipe = -1;
        free(this->connection[i]->buf);
        this->connection[i]->buf = NULL;

        free(this->connection[i]);
        this->connection[i] = NULL;
    }

    free(this->connection);
    this->connection = NULL;

dserver:
    this->channel.readFd = -1;
    this->channel.writeFd = -1;

    this->n = 0;

    free(this);
    this == NULL;

    return 0;
}

int initServer(long n, int fileFd)
{
    server_t* server = (server_t*) calloc(1, sizeof(server_t));
    if(server == NULL)
        return NULL;

    double dsize = 512 * pow(3, n);

    int size = (int) dsize;

    server->connection = (connection_t**) calloc(n, sizeof(connection_t*));
    if(server->connection == NULL)
    {
        deleteServer(server);
        return NULL;
    }

    int* inpipes[2] = (int*) calloc(n, sizeof(int));
    int* outpipes[2] = (int*) calloc(n, sizeof(int));


    long i;
    for(i = 0; i < n; i++)
    {
        /*int fdIn[2];
        int fdOut[2];*/

        pipe(inpipes[i]);
        pipe(outpipes[i]);

       
        pid_t pid = fork();

        /*server->inpipe[R] = fdIn[R];
        server->inpipe[W] = fdIn[W];
        server->outpipe[R] = fdOut[R];
        server->outpipe[W] = fdOut[W];*/


        if(pid == 0)
        {
            close(inpipes[i][W]);
            close(outpipes[i][R]);

            if(i == 0)
            {
                initChannel(&(server->channel), fileFd, outpipes[i][W]);
                close(inpipes[i][R]);
            }
            
            if(0 < i && i < n - 1)
            {
                initChannel(&(server->channel), inpipes[i][R], outpipes[i][W]);
            }

            if(i == n -1)
            {
                initChannel(&(server->channel), inpipes[i][R], STDOUT_FILENO);
                close(outpipes[i][W]);
            }

        }
    }

    for(i = 0; i < n; i++)
    {
        
    }

    
    /*long i;
    for(i = 0; i < n; i++)
    {
        int fdIn[2];
        int fdOut[2];

        pipe(fdIn);
        pipe(fdOut);

        pid_t pid = fork();

                
        if(pid == 0)
        {
            close(fdIn[W]);
            close(fdOut[R]);

            if(i == 0)
            {
                initChannel(&(server->channel), fileFd, fdOut[W]);
                close(fdIn[R]);
            }
            
            if(0 < i && i < n - 1)
            {
                initChannel(&(server->channel), fdIn[R], fdOut[W]);
            }

            if(i == n -1)
            {
                initChannel(&(server->channel), fdIn[R], STDOUT_FILENO);
                close(fdOut[W]);
            }

            deleteServerConnection(server, i);
            free(server->connection);
            server->connection = NULL;

            break;

        }

        if(pid > 0)
        {
            close(fdIn[R]);
            close(fdOut[W]);

            if(i == n - 1)
            {
                server->connection[i - 1]->wpipe = fdIn[W];
                close(fdOut[R]);
                break;
            }

            server->connection[i] = (connection_t*) calloc(1, sizeof(connection_t));
            if(server->connection[i] == NULL)
            {
                deleteServer(server);
                return NULL;
            }

            server->connection[i]->size = size;
            server->connection[i]->buf = (char*) calloc(size, sizeof(char));
            if(server->connection[i]->buf == NULL)
            {
                deleteServer(server);
                return NULL;
            }
                

            if(i == 0)
            {
                server->connection[i]->rpipe = fdOut[R];
                close(fdIn[W]);
            }

            if(0 < i && i < n - 1)
            {
                server->connection[i]->rpipe = fdOut[R];
                server->connection[i-1]->wpipe = fdIn[W];
            }
            
        }

        size /= 3;

    }*/

    return server;
}




int main(int argc, char* argv[])
{
    int fd = 0;
    long n = handle(argc, argv, &fd);

    

    //printf("%d\n", ret);

    close(fd);
    return 0;
}


