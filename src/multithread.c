#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define FIB_DEV "/dev/fibonacci"
#define NTHREAD 2

void *(thread_handler) (void *x)
{
    long long sz;

    char buf[256];
    int offset = 1000; /* TODO: try test something bigger than the limit */

    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open character device");
        exit(1);
    }

    for (int i = 0; i <= offset; i++) {
        lseek(fd, i, SEEK_SET);
        // cppcheck-suppress unreadVariable
        sz = read(fd, buf, 1);
        printf("Reading from " FIB_DEV
               " at offset %d, returned the sequence "
               "%s.\n",
               i, buf);
    }

    for (int i = offset; i >= 0; i--) {
        lseek(fd, i, SEEK_SET);
        // cppcheck-suppress unreadVariable
        sz = read(fd, buf, 1);
        printf("Reading from " FIB_DEV
               " at offset %d, returned the sequence "
               "%s.\n",
               i, buf);
    }

    close(fd);
}

int main()
{
    pthread_t pt[NTHREAD];
    for (int i = 0; i < NTHREAD; i++)
        pthread_create(&pt[i], NULL, thread_handler, NULL);
    for (int i = 0; i < NTHREAD; i++)
        pthread_join(pt[i], NULL);
    return 0;
}
