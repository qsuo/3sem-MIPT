#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

int check(int fd1, int fd2)
{
    struct stat buf1;
    struct stat buf2;

    fstat(fd1, &buf1);
    fstat(fd2, &buf2);

    if( (buf1.st_ino == buf2.st_ino) && (buf1.st_dev == buf2.st_dev) )
        return 1;

    return 0;
}

int main(int argc, char* argv[])
{
    int fd1 = open(argv[1], O_RDWR);
   
    int fd2 = open(argv[2], O_RDWR);

    int res = check(fd1, fd2);

    printf("%d\n", res);

    return 0;
}
