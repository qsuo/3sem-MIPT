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
#include <sys/shm.h>
#include <sys/sem.h>

#define DATASIZE 216
#define SEMNUMBER 8
#define SEMOPERS 3
#define MAXOPERS 5

#define ADD_P(operNum, semNum, flag)\
    semoper[operNum].sem_num = semNum;\
    semoper[operNum].sem_flg = flag;\
    semoper[operNum].sem_op = -1;

#define ADD_V(operNum, semNum, flag)\
    semoper[operNum].sem_num = semNum;\
    semoper[operNum].sem_flg = flag;\
    semoper[operNum].sem_op = 1;

#define ADD_Z(operNum, semNum, flag)\
    semoper[operNum].sem_num = semNum;\
    semoper[operNum].sem_flg = flag;\
    semoper[operNum].sem_op = 0;


#define DO_P(semNum, flag)\
    semoper[0].sem_num = semNum;\
    semoper[0].sem_flg = flag;\
    semoper[0].sem_op = -1;\
    semop(semid, semoper, 1);
    
#define DO_V(semNum, flag)\
    semoper[0].sem_num = semNum;\
    semoper[0].sem_flg = flag;\
    semoper[0].sem_op = 1;\
    semop(semid, semoper, 1);

#define P -1
#define V 1
#define Z 0

#define ADD_OP(operNum, semNum, oper, flag)\
    semoper[operNum].sem_num = semNum;\
    semoper[operNum].sem_flg = flag;\
    semoper[operNum].sem_op = oper;\



enum
{
    MUTEX_R,//0
    MUTEX_S,//1
    EMPTY,//2
    FULL,//3
    READ,//4
    WRITE,//5
    ALIVE_R,//6
    ALIVE_S//7
};

union semun
{
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

#define CASEPRINT(what)\
    case what:\
        printf("["#what"] = %d\n", array[i]);\
        break

int semDump(int semid)
{   

    union semun arg;

    unsigned short array[SEMNUMBER] = {};
    arg.array = array;

    semctl(semid, 0, GETALL, arg);

    int i;
    for(i = 0; i < SEMNUMBER; i++)
    {
        switch(i)
        {
            CASEPRINT(MUTEX_R);
            CASEPRINT(MUTEX_S);
            CASEPRINT(EMPTY);
            CASEPRINT(FULL);
            CASEPRINT(READ);
            CASEPRINT(WRITE);
            CASEPRINT(ALIVE_R);
            CASEPRINT(ALIVE_S);
            default:
                printf("[%d](Unknown semaphore) = %d\n", i, array[i]);
        }
    }

    return 0;
}

#undef CASEPRINT

int semSet(int semid, int semnum, unsigned short value)
{
    union semun arg;

    arg.val = value;

    semctl(semid, semnum, SETVAL, arg);

    return 0;
}

typedef struct msg_s
{
    long size;
    char buf[DATASIZE];
} msg_t;

const long SHMSIZE = sizeof(msg_t);



int sender(int semid, msg_t* msg, const char* fileName)
{
    struct sembuf semoper[MAXOPERS] = {};


    /*ADD_Z(0, MUTEX_S, IPC_NOWAIT);
    ADD_V(1, MUTEX_S, SEM_UNDO);*/

    ADD_OP(0, MUTEX_S, Z, IPC_NOWAIT);
    ADD_OP(1, MUTEX_S, V, SEM_UNDO);

    if(semop(semid, semoper, 2) == -1)
    {
        fprintf(stderr, "Another sender is active\n");
        exit(EXIT_FAILURE);
    }

    DO_V(ALIVE_S, SEM_UNDO);


    int fd = open(fileName, O_RDONLY);
    if(fd == -1)
    {
        fprintf(stderr, "No such file\n");
        exit(EXIT_FAILURE);
    }
   

    DO_V(READ, SEM_UNDO);
    DO_P(WRITE, 0);

    long count = DATASIZE;
    while(count > 0)
    {

        ADD_P(0, ALIVE_R, IPC_NOWAIT);
        ADD_V(1, ALIVE_R, 0);
        ADD_P(2, EMPTY, 0);

        if(semop(semid, semoper, 3) == -1)
            exit(EXIT_FAILURE);

        count = read(fd, msg->buf, DATASIZE);
        msg->size = count;

        DO_V(FULL, 0);

    }

    return 0;
}

int receiver(int semid, msg_t* msg)
{
   
    struct sembuf semoper[MAXOPERS] = {};


    ADD_Z(0, MUTEX_R, IPC_NOWAIT);
    ADD_V(1, MUTEX_R, SEM_UNDO);

    DO_V(ALIVE_R, SEM_UNDO);

    if(semop(semid, semoper, 2) == -1)
    {
        fprintf(stderr, "Another receiver is active\n");
        exit(EXIT_FAILURE);
    }

    semSet(semid, EMPTY, 1);
    semSet(semid, FULL, 0);


    DO_V(WRITE, SEM_UNDO);
    DO_P(READ, 0);



    long count = DATASIZE;
    while(count > 0)
    {


        ADD_P(0, ALIVE_S, IPC_NOWAIT);
        ADD_V(1, ALIVE_S, 0);
        ADD_P(2, FULL, 0);

        if(semop(semid, semoper, 3) == -1)
            exit(EXIT_FAILURE);

        
        count = msg->size;
        write(STDOUT_FILENO, msg->buf, count);
        

        DO_V(EMPTY, 0);
    }

    return 0;
}


int main(int argc, char* argv[])
{
    if(argc > 3)
    {
        fprintf(stderr, "Invalid input\n");
        return 0;
    }

    key_t key = ftok("./", 1);
    
    int shmid = shmget(key, SHMSIZE, IPC_CREAT | 0666);

    if(shmid == -1)
    {
        fprintf(stderr, "Can't allocate shared memory\n");
        goto exit;
    }

    msg_t* msg = shmat(shmid, NULL, 0);
    if((long)msg == -1)
    {
        fprintf(stderr, "Can't attach shared memory\n");
        goto shmat;
    }

    int semid = semget(key, SEMNUMBER, IPC_CREAT | 0666);
    if(semid == -1)
    {
        fprintf(stderr, "Can't create semaphores\n");
        goto semget;
    }


    if(argc == 1)
        receiver(semid, msg);
    else if(argc == 2)
        sender(semid, msg, argv[1]);
    
    semctl(semid, SEMNUMBER, IPC_RMID);
semget:
    shmdt(msg);
shmat:
    shmctl(shmid, IPC_RMID, NULL);
exit:
    return 0;
}
