
/* StepperDriver.cpp:
 * Program to read a stepper motor speed from a file (in steps/sec) and output through Beaglebone * GPIO interface.
 * Running as part of a separate process in order to write the pulse at as smooth of a speed as  * possible.

 */


#include <iostream>
#include <iomanip>
#include "motor/StepperMotor.h"
#include <time.h>
#include <stdint.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>


// Constants for Serial transmission to Arduino
#define NETWORK_SIG_SIZE 3

#define VAL_SIZE         2
#define CHECKSUM_SIZE    1
#define PACKET_SIZE      (NETWORK_SIG_SIZE + VAL_SIZE + CHECKSUM_SIZE)

// The network address byte and can be change if you want to run different devices in proximity to each other without interfearance
#define NET_ADDR 5

const char g_network_sig[NETWORK_SIG_SIZE] = {0x8F, 0xAA, NET_ADDR};  // Few bytes used to initiate a transfer




// Other constants
#define BILLION 1000000000L
#define MILLION 1000000L
#define FILTERLENGTH  7

using namespace std;
using namespace exploringBB;

volatile sig_atomic_t flag = 0;
void signalInterrupt(int sig) { // can be called asynchronously
    flag = 1; // set flag
}


/* elapsedNanos
Calculate time in nanosecs from two timespecs
*/
uint64_t elapsedNanos(timespec start, timespec end) {
    return BILLION * (end.tv_sec - start.tv_sec) + end.tv_nsec - start.tv_nsec;
}

// rc interrupt fifo
char * fifoPipes[] = {
    "/tmp/fifo0",
    "/tmp/fifo1",
    "/tmp/fifo2",
    "/tmp/fifo3",
    "/tmp/fifo4",
    "/tmp/fifo5"
};
int rcChannelCmd[6];
int rcChannelFilter[6][FILTERLENGTH];
int rcOn[6] = {0,0,1,0,1,0};
int forwardCh = 2;
int panCh = 4;

int filterRcChannel(int channel, int val, int (&array)[6][FILTERLENGTH]) {
    int sum = 0;
    int divisor = 1;
    for (int i =0; i<FILTERLENGTH -1; i++) {
        array[channel][i] = array[channel][i+1];
        sum = sum + array[channel][i+1];
        if (array[channel][i] != 0) divisor++;
    }
    array[channel][FILTERLENGTH-1] = val;
    sum = sum + val;
    return sum/divisor;
}


/* writeUInt function:
*/
// Sends an unsigned int over the RF network
void writeUInt(int file, unsigned int val)
{
    char checksum = (val/256) ^ (val&0xFF);
    int count,num;
    num = 0;
    char syncVal = 0xF0;
    char perr[80];

    count = write(file, &syncVal, sizeof(syncVal));  // This gets reciever in sync with transmitter
    if (count < 0) num = num + 1;
    count = write(file, g_network_sig, NETWORK_SIG_SIZE);
    if (count < 0) num = num + 2;
    count = write(file, (unsigned char*)&val, VAL_SIZE);
    if (count < 0) num = num + 3;
    count = write(file, &checksum,sizeof(checksum)); //CHECKSUM_SIZE
    if (count < 0) num = num + 4;
    if (num > 0) {
        sprintf(perr,"Failed to write to the output: %d \n",num);
        perror(perr);
    }
}





int main() {
    cout << "Starting Stepper Driver Program:" << endl;

    // Register signals
    signal(SIGINT, signalInterrupt);

    // inputs
    int speedRPM = 60; // RPM
    int maxSpeed = 100;
    int stepsPerRev = 200 * 10; // 200 steps per revolution * 10 microsteps/step
    int dir = 1; // Clockwise
    int pulsePerSec = stepsPerRev * speedRPM / 60; // (pulse/rev) * (rev/min) * (1 min/60 sec)

    // GPIO pin numbers
    int pinMS1 = 16; // Not necessary, so disable it
    int pinMS2 = 16; // Not necessary so disable it
    int pinStep = 26; // P8_14 GPIO0_26
    int pinSleep = 45; // P8_11 GPIO1_13
    int pinDir = 44; // P8_12 GPIO1_12


    // pan variables
    int panSpeed;
    unsigned int uint_panSpeed;
    int sio_file;
    if ((sio_file = open("/dev/tty4", O_RDWR | O_NOCTTY | O_NDELAY))<0) {
        perror("UART: Failed to open the file.\n");
        return -1;
    }
    struct termios options;
    tcgetattr(sio_file, &options);
    options.c_cflag = B9600 | CS8 | CLOCAL;
    options.c_iflag = IGNPAR | ICRNL;
    tcflush(sio_file, TCIFLUSH);
    tcsetattr(sio_file, TCSANOW, &options);



    // upkeep variables
    struct timespec start, last, elapsed, endloop;
    clock_gettime(CLOCK_REALTIME,&start);
    last = start;

    uint64_t telapsed;
    float tfloat;

    // read input speed (pulse/sec)
    int fifo_fd = open("/tmp/stepfifo",O_RDONLY | O_NONBLOCK);
    if (fifo_fd < 1) {
        printf("Error in opening /tmp/stepfifo - ");
        printf("Error code: %d\n",fifo_fd);
        return -1;
    }
    int temp = 0;
    read(fifo_fd,&temp,sizeof(int));
    close(fifo_fd);

    speedRPM = (int) (60 * pulsePerSec / stepsPerRev);

    // zero filter arrays
    for (int i=0; i < 6; i++) {
        for (int j=0; j< FILTERLENGTH; j++) {
            rcChannelFilter[i][j] = 0;
        }
    }

    // read RC commands
    int rc_fd[6];
    for (int i=0; i<6; i++) {
        if (rcOn[i]) {
            rc_fd[i] = open(fifoPipes[i],O_RDONLY | O_NONBLOCK);
            read(rc_fd[i],&rcChannelCmd[i],sizeof(int));
            int temp = filterRcChannel(i,rcChannelCmd[i],rcChannelFilter);
            rcChannelCmd[i] = temp;
        }
    }

    int counter,checkForRCcount;
    checkForRCcount = 5;
    counter = 0;
    //Setup stepper motor object
    StepperMotor m(pinMS1,pinMS2,pinStep,pinSleep,pinDir,speedRPM,stepsPerRev);


    // Loop upkeep vars
    int looptime_usec = 0.04 * 1000000; // 25 Hz (0.04 sec) -> microseconds
    int tleft_usec = 0;
    int timestepsToCmd = 1;

    while (1) {
        if (flag) {
            break;
        }
        last = elapsed;
        clock_gettime(CLOCK_REALTIME,&elapsed);
        telapsed = elapsedNanos(last,elapsed);
        tfloat = (float) (telapsed/BILLION);
        printf("elapsed time: %f\n",tfloat);

        if (counter % checkForRCcount == 0) {
            // check for RC signals
            for (int i =0; i<6; i++) {
                if (rcOn[i]) {
                    read(rc_fd[i],&rcChannelCmd[i],sizeof(int));
                    int temp = filterRcChannel(i,rcChannelCmd[i],rcChannelFilter);
                    rcChannelCmd[i] = temp;
                }
            }
            counter = 0;

            // get speed, scale of 1000-1500 (neg), 1500-2000 (pos)
            speedRPM = (rcChannelCmd[forwardCh] - 1500) * maxSpeed / 500;
            cout << "rcChannelCmd[2]: " << rcChannelCmd[2] << endl;
            if (speedRPM < 0) {
                m.setDirection(StepperMotor::ANTICLOCKWISE);
                speedRPM = -1 * speedRPM;
            } else {
                m.setDirection(StepperMotor::CLOCKWISE);
            }
            m.setSpeed(speedRPM);

            // get pan speed
            panSpeed = rcChannelCmd[panCh];
            cout << "speedRPM: " << speedRPM << ", panSpeed: " << panSpeed << endl;
            uint_panSpeed = (unsigned int) (panSpeed);
            writeUInt(sio_file,uint_panSpeed);

        } else {
            if (counter == checkForRCcount - 1) {
                timeStepsToCmd = 2;
            } else {
                timeStepsToCmd = 1;
            }
            int numberOfSteps = timeStepsToCmd * speedRPM * stepsPerRev / 60 * looptime_usec/1000000;
            m.threadedStepForDuration(numberOfSteps,(int) (looptime_usec/1000)); // Send in milliseconds
        }

        clock_gettime(CLOCK_REALTIME,&endloop);
        telapsed = elapsedNanos(elapsed,endloop);
        tleft_usec = looptime_usec - (telapsed/1000);
        usleep(tleft_usec);

        counter++;
    }

    for (int i=0; i<6; i++) {
        close(rc_fd[i]);
    }

    m.sleep();



}
