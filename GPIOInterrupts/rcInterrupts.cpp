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

volatile int rxChannelCmd[6];
volatile struct timespec ts[6];

#define BILLION 1000000000L
#define MICRO2NANO 1000L 


uint64_t elapsedNanos(timespec start, timespec end) {
	return BILLION * (end.tv_sec - start.tv_sec) + end.tv_nsec - start.tv_nsec;
}



static gboolean
calcRXInput( GIOChannel *channel,
               GIOCondition condition,
               gpointer user_data )
{
    int *i = (int*) user_data; // need to define which channel we're processing
    struct timespec oldTs = ts[i];

    // get the new time as soon as we can!
	clock_gettime(CLOCK_REALTIME,&ts[i]);
	uint64_t elapsedTime = elapsedNanos(oldTs,ts[i]);


    cerr << "calcRXInput" << endl;
    GError *error = 0;
    gsize bytes_read = 0; 
    const int buf_sz = 1024;
    gchar buf[buf_sz] = {};

    g_io_channel_seek_position( channel, 0, G_SEEK_SET, 0 );
    GIOStatus rc = g_io_channel_read_chars( channel,
                                            buf, buf_sz - 1,
                                            &bytes_read,
                                            &error );
    cerr << "rc:" << rc << "  data:" << buf << endl;

    // If we're low (and the command is valid), save the command
    if ((g_ascii_strtoll(buf) < 1) && elapsedTime > 999 && elapsedTime < 2001) 
    {
    	rxChannelCmd[i] = (int) elapsedTime;

    }
    
    // thank you, call again!
    return 1;
}

int main( int argc, char** argv )
{
    GMainLoop* loop = g_main_loop_new( 0, 0 );
    int fd[6];
    GIOChannel* channel[6];
    GIOCondition cond[6];

    // GPIO paths (need to be updated and checked)
    char * gpioPaths[] = {
    	"/sys/class/gpio/gpio70/value",
    	"/sys/class/gpio/gpio71/value",
    	"/sys/class/gpio/gpio72/value",
    	"/sys/class/gpio/gpio73/value",
    	"/sys/class/gpio/gpio74/value",
    	"/sys/class/gpio/gpio75/value"
    };

    // Loop over 6 RX Channels
    for (int i = 0; i<6; i++) {
    	fd[i] = open( gpioPaths[i], O_RDONLY | O_NONBLOCK );
    	channel[i] = g_io_channel_unix_new( fd );
    	cond[i] = GIOCondition( G_IO_PRI );
    	guint id = g_io_add_watch( channel, cond, calcRXInput, (gpointer) &i);
    }
    
    
    g_main_loop_run( loop );
}
