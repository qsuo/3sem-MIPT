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

typedef struct server_s
{
    long n;
    connection_t** connection;
    channel_t channel;
} server_t;


/*connection_t* createConnection(size_t size, int rpipe, int wpipe)
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
}*/

/*int deleteConnection(connection_t* this)
{
    this->size = 0;
    this->rpipe = 0;
    this->wpipe = 0;
    free(this->buf);
    this->buf = NULL;
    free(this);
    this = NULL;
    return 0;
}*/


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

enum file_d
{
    R = 0,
    W
};

server_t* createServer(long n, int fileFd)
{
    server_t* server = (server_t*) calloc(1, sizeof(server_t));
    if(server == NULL)
        return NULL;

    double dsize = 512 * pow(3, n);

    int size = (int) dsize;

    server->connection = (connection_t**) calloc(n, sizeof(connection_t*));
    if(server->connection == NULL)
        return NULL;

    
    long i;
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
        }

        if(pid > 0)
        {
            close(fdIn[R]);
            close(fdOut[W]);

            if(i == n - 1)
            {
                server->connection[i - 1]->wpipe = fdIn[W];
                close(fdOut[R]);
            }

            server->connection[i] = (connection_t*) calloc(1, sizeof(connection_t));
            server->connection[i]->size = size;
            server->connection[i]->buf = (char*) calloc(size, sizeof(char));
                

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

    }
    

}


int main(int argc, char* argv[])
{
    int fd = 0;
    long n = handle(argc, argv, &fd);

    return 0;
}


