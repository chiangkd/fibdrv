#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <time.h>
#define __USE_GNU
#include <unistd.h>


#define FIB_DEV "/dev/fibonacci"
#define CLOCKID CLOCK_MONOTONIC
#define NTHREAD 10

pthread_mutex_t plock;

long get_ntime()
{
    struct timespec tmp;
    clock_gettime(CLOCKID, &tmp);
    return tmp.tv_sec * 1e9 + tmp.tv_nsec;
}

void *thread_handler(void *x)
{

    char buf[256];
    char write_buf[] = "testing writing";
    int offset = 1000; /* TODO: try test something bigger than the limit */

    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        printf("thread ID - %d Failed to open character device\n", ktid);
        // perror("Failed to open character device");
        exit(1);
    }

    for (int i = 0; i <= offset; i++) {
        long ut, kt;

        lseek(fd, i, SEEK_SET);
        pthread_mutex_lock(&plock);
        ut = get_ntime();
        read(fd, buf, 1);
        ut = get_ntime() - ut;
        kt = write(fd, write_buf, strlen(write_buf));
        pthread_mutex_unlock(&plock);
        printf("Time = %ld %ld %ld\n", ut, kt,
               ut - kt);  // user | kernel | kernel to user
    }
    close(fd);
}

int main()
{
    pthread_t pt[NTHREAD];
    for (int i = 0; i < NTHREAD; i++) {
        pthread_create(&pt[i], NULL, thread_handler, NULL);
        // printf("Create thread ID = %ld\n", pt[i]);
    }
    for (int i = 0; i < NTHREAD; i++) {
        pthread_join(pt[i], NULL);
        // printf("Join thread ID = %ld\n", pt[i]);
    }
    return 0;
}
