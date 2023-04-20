#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define FIB_DEV "/dev/fibonacci"
#define CLOCKID CLOCK_MONOTONIC

long get_ntime()
{
    struct timespec tmp;
    clock_gettime(CLOCKID, &tmp);
    return tmp.tv_sec * 1e9 + tmp.tv_nsec;
}

int main()
{
    char buf[10000];
    char write_buf[] = "testing writing";
    int offset = 10000; /* TODO: try test something bigger than the limit */

    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open character device");
        exit(1);
    }

    for (int i = 0; i <= offset; i++) {
        long ut, kt;

        lseek(fd, i, SEEK_SET);
        ut = get_ntime();
        read(fd, buf, 1);
        ut = get_ntime() - ut;
        kt = write(fd, write_buf, strlen(write_buf));
        printf("Time = %ld %ld %ld\n", ut, kt,
               ut - kt);  // user | kernel | kernel to user
    }

    for (int i = offset; i >= 0; i--) {
        long ut, kt;

        lseek(fd, i, SEEK_SET);
        ut = get_ntime();
        read(fd, buf, 1);
        ut = get_ntime() - ut;
        kt = write(fd, write_buf, strlen(write_buf));
        printf("Time = %ld %ld %ld\n", ut, kt,
               ut - kt);  // user | kernel | kernel to user
    }

    close(fd);
    return 0;
}
