/* RC Interrupts - 
Beaglebone Black GPIO

written by: Jason Antico
*/

#include <glib.h>
#include <fcntl.h>
#include <iostream>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <signal.h>


volatile int rxChannelCmd[6];
volatile int lastChannelCmd[6];

//volatile struct timespec ts[6];
//volatile struct timespec oldTs;

struct pseudoTs {
	uint64_t tv_sec;
	uint64_t tv_nsec;
};

volatile pseudoTs ts[6];
static volatile int keepRunning = 1;

void intHandler(int dummy) {
    	keepRunning = 0;
}

#define BILLION 1000000000L
#define MICRO2NANO 1000L 

uint64_t elapsedNanos(timespec start, timespec end) {
	return BILLION * (end.tv_sec - start.tv_sec) + end.tv_nsec - start.tv_nsec;
}

uint64_t elapsedNanos(pseudoTs start, timespec end) {
        return BILLION * (end.tv_sec - start.tv_sec) + end.tv_nsec - start.tv_nsec;
}

char * fifoPipes[] = {
	"/tmp/fifo0",
	"/tmp/fifo1",
	"/tmp/fifo2",
	"/tmp/fifo3",
	"/tmp/fifo4",
	"/tmp/fifo5"
};

int rcOn[] = {0,1,1,0,1,0};


int fifo_fd[6];

GMainLoop* loop;


static gboolean
calcRXInput( GIOChannel *channel,
               GIOCondition condition,
               gpointer user_data )
{
    glong gi;
    gi = GPOINTER_TO_INT(user_data);
    int i = (int) gi;
    //int *p = (int*) user_data; // need to define which channel we're processing
    //int i = *p;
    pseudoTs oldTs;
    oldTs.tv_sec = ts[i].tv_sec;
    oldTs.tv_nsec = ts[i].tv_nsec;

    struct timespec currTs;
    // get the new time as soon as we can!
	clock_gettime(CLOCK_REALTIME,&currTs);
	uint64_t elapsedTime = elapsedNanos(oldTs,currTs);
    ts[i].tv_sec = currTs.tv_sec;
    ts[i].tv_nsec = currTs.tv_nsec;

//    std::cerr << "calcRXInput" << std::endl;
    GError *error = 0;
    gsize bytes_read = 0; 
    const int buf_sz = 1024;
    gchar buf[buf_sz] = {};

    g_io_channel_seek_position( channel, 0, G_SEEK_SET, 0 );
    GIOStatus rc = g_io_channel_read_chars( channel,
                                            buf, buf_sz - 1,
                                            &bytes_read,
                                            &error );
//    std::cerr << "rc:" << rc << "  data:" << buf << std::endl;

    // If we're low (and the command is valid), save the command
    elapsedTime = elapsedTime/MICRO2NANO;    
    if ((g_ascii_strtoll(buf,NULL,10) < 1) && elapsedTime > 999 && elapsedTime < 3001) 
    {
    	rxChannelCmd[i] = (int) elapsedTime;
	std::cout << "Ch:" << i << ", val:" << rxChannelCmd[0] << "," << rxChannelCmd[1] << ",";
	std::cout << rxChannelCmd[2] << "," << rxChannelCmd[3] << "," << rxChannelCmd[4] << ",";
        std::cout << rxChannelCmd[5] << std::endl;
	int val = rxChannelCmd[i];
    if (rcOn[i]) write(fifo_fd[i],&val, sizeof(val));
//    usleep(1000);
    }
    


    if (keepRunning) {
    // thank you, call again!
    return 1;
    } else {
	g_main_loop_quit(loop);
	return 0;
    }
}

int main( int argc, char** argv )
{
    loop = g_main_loop_new( 0, 0 );
    int fd[6];
    GIOChannel* channel[6];
    GIOCondition cond[6];
    glong gi;
    gpointer gp;

    signal(SIGINT, intHandler);

    // GPIO paths (need to be updated and checked)
    char * gpioPaths[] = {
    	"/sys/class/gpio/gpio75/value",
    	"/sys/class/gpio/gpio74/value",
    	"/sys/class/gpio/gpio73/value",
    	"/sys/class/gpio/gpio72/value",
    	"/sys/class/gpio/gpio71/value",
    	"/sys/class/gpio/gpio70/value"
    };

    // Loop over 6 RX Channels
    for (int i = 0; i<6; i++) {
    	fd[i] = open( gpioPaths[i], O_RDONLY | O_NONBLOCK );
    	channel[i] = g_io_channel_unix_new( fd[i] );
    	cond[i] = GIOCondition( G_IO_PRI );
	gi = (glong) i;
	gp = GINT_TO_POINTER(gi);

    	guint id = g_io_add_watch( channel[i], cond[i], calcRXInput, gp);
     	std::cout << i << ",fd: " << fd[i] << std::endl;

	// make our FIFO output pipes
	int tempfifo;
    tempfifo = mkfifo(fifoPipes[i], 0666);
	if (tempfifo < 0) {
        printf("Unable to create a fifo");
        exit(-1);
    }
    fifo_fd[i] = open(fifoPipes[i],O_RDWR);
    if (fifo_fd[i] < 1) {
    printf("Error opening fifo%d",i);
    printf("Error code: %d",fifo_fd[i]);
    unlink(fifoPipes[i]);
    }
    }
    
    std::cout << "Entering the loop" << std::endl;    
    g_main_loop_run( loop );
    std::cout << "Left the loop?" << std::endl;
    for (int i = 0; i<6; i++) {
	close(fifo_fd[i]);
	unlink(fifoPipes[i]);
    }

}
