#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/types.h>
#define __USE_GNU
#include <unistd.h>


#define FIB_DEV "/dev/fibonacci"
#define NTHREAD 10

pthread_mutex_t plock;

void *thread_handler(void *x)
{
    // cppcheck-suppress variableScope
    long long sz;

    int ktid = syscall(__NR_gettid);

    char buf[256];
    int offset = 1000; /* TODO: try test something bigger than the limit */

    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        printf("thread ID - %d Failed to open character device\n", ktid);
        // perror("Failed to open character device");
        exit(1);
    }

    for (int i = 0; i <= offset; i++) {
        lseek(fd, i, SEEK_SET);
        // cppcheck-suppress unreadVariable
        sz = read(fd, buf, 1);
        printf("TID = %d, Reading from " FIB_DEV
               " at offset %d, returned the sequence "
               "%s.\n",
               ktid, i, buf);
    }
    close(fd);
}

int main()
{
    pthread_t pt[NTHREAD];
    for (int i = 0; i < NTHREAD; i++) {
        pthread_create(&pt[i], NULL, thread_handler, NULL);
        printf("Create thread ID = %ld\n", pt[i]);
    }
    for (int i = 0; i < NTHREAD; i++) {
        pthread_join(pt[i], NULL);
        printf("Join thread ID = %ld\n", pt[i]);
    }
    return 0;
}
