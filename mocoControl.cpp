#include <time.h>
#include <stdint.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#define BILLION 1000000000L

char * fifoPipes[] = {
    "/tmp/fifo0",
    "/tmp/fifo1",
    "/tmp/fifo2",
    "/tmp/fifo3",
    "/tmp/fifo4",
    "/tmp/fifo5"
};

int main(void) {
    int fifo_fd[6];
    char *buf;
    int rcIn[6];

    for (int i=0; i<6; i++) {
        printf("%s\n",fifoPipes[i]);
        fifo_fd[i]=open(fifoPipes[i],O_RDONLY | O_NONBLOCK);
        if (fifo_fd[i] < 1) {
            printf("Error in opening fifo%d\n",i);
            printf("Error code: %d\n",fifo_fd[i]);
            exit(-1);
        } 
        printf("done\n");
    }

    printf("Loaded everything\n");

    long count=0;
    while(count < 100000){
        for (int i=0; i < 6; i++) {
    //        printf("first read\n");
            read(fifo_fd[i],&rcIn[i],sizeof(int));
            printf("fifo%d: %d\n",i,rcIn[i]);
        }
        count++;
    }

    for (int i=0; i<6;i++){
        close(fifo_fd[i]);
    }

    return 0;

}
