
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
#include <string.h>
#include <stdlib.h>


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
#define FILTERLENGTH  11

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
int rcOn[6] = {0,1,1,0,1,0};
int forwardCh = 2;
int panCh = 4;
int autoCh = 1;

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


void readOffRcCmds(int rc_fd[6]) {
    int temp;
    for (int i = 0; i< 6; i++) {
        if (rcOn[i]) {
            temp = rcChannelCmd[i];
//            cout << "while loop i: " << i << endl;
            int readerr = 0;
            while((readerr = read(rc_fd[i],&temp,sizeof(int))) > 0) {
                // do nothing!
//                cout << i << "temp: " << temp << endl;
            }
//            cout << "read err: " << readerr << endl;
            rcChannelCmd[i] = filterRcChannel(i,temp,rcChannelFilter);
//            cout << "Filtered : " << rcChannelCmd[i] << endl;
        }
    }    
//    cout << "Done reading" << endl;
}




int main() {
    cout << "Starting Stepper Driver Program:" << endl;

    // Register signals
    signal(SIGINT, signalInterrupt);

    // inputs
    int speedRPM = 100; // RPM
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
    if ((sio_file = open("/dev/ttyO4", O_RDWR | O_NOCTTY | O_NDELAY))<0) {
        perror("UART: Failed to open the file.\n");
        return -1;
    }
    int count;
    char str[10];


    struct termios options;
    tcgetattr(sio_file, &options);
    options.c_cflag = B2400 | CS8 | CLOCAL;
    options.c_iflag = IGNPAR | ICRNL;
    tcflush(sio_file, TCIFLUSH);
    tcsetattr(sio_file, TCSANOW, &options);
    unsigned char transmit[20] = "0x444";
    write(sio_file,&transmit,sizeof(transmit) + 1);

    // upkeep variables
    struct timespec start, last, elapsed, endloop;
    clock_gettime(CLOCK_REALTIME,&start);
    last = start;

    uint64_t telapsed;
    float tfloat;

    // read input speed (pulse/sec)

/*    int fifo_fd = open("/tmp/stepfifo",O_RDONLY | O_NONBLOCK);
    if (fifo_fd < 1) {
        printf("Error in opening /tmp/stepfifo - ");
        printf("Error code: %d\n",fifo_fd);
        return -1;
    }
    int temp = 0;
    read(fifo_fd,&temp,sizeof(int))
*/
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
    cout << "Reading off RC cmds" << endl;
    readOffRcCmds(rc_fd);
    cout << "Done reading RC cmds" << endl;

    int counter,checkForRCcount;
    checkForRCcount = 5;
    counter = 0;
    //Setup stepper motor object
    StepperMotor m(pinMS1,pinMS2,pinStep,pinSleep,pinDir,speedRPM,stepsPerRev);


    // Loop upkeep vars
    int looptime_usec = 0.04 * 1000000; // 25 Hz (0.04 sec) -> microseconds
    int tleft_usec = 0;
    int timeStepsToCmd = 1;
    
    // autonomous mode vars
    bool bAutonomous = 0;
    int autonomousSpeed = 10;
    int autoPanSpeed = 0;
    int autoRcThresh = 1500;
    StepperMotor::DIRECTION autoDir = StepperMotor::CLOCKWISE;
    long autoTime = 0;
    long timeToPan = 5 * 60 * 1000000; // 5 min * 60 sec * 1000000 usec


    // vars for endstops
    bool endStop = 0;
    StepperMotor::DIRECTION endstopDir = StepperMotor::CLOCKWISE;
    StepperMotor::DIRECTION currentDir = StepperMotor::CLOCKWISE;
    m.sleep();

    while (1) {
        if (flag) {
            break;
        }
        last = elapsed;
        clock_gettime(CLOCK_REALTIME,&elapsed);
        telapsed = elapsedNanos(last,elapsed);
        printf("telapsed: %d \n",(int) (telapsed/1000));
//        tfloat = (float) (telapsed/1000000000);
//        printf("elapsed time: %f\n",tfloat);
//        cout << "elapsed time: " << fixed << setprecision(5) << tfloat <<endl;

        if (counter % checkForRCcount == 0) {
            // check for RC signals
/*            for (int i =0; i<6; i++) {
                if (rcOn[i]) {
                    read(rc_fd[i],&rcChannelCmd[i],sizeof(int));
                    int temp = filterRcChannel(i,rcChannelCmd[i],rcChannelFilter);
                    rcChannelCmd[i] = temp;
                }
            }
*/
            readOffRcCmds(rc_fd);
            counter = 0;
            
            // check for autonomous mode
            if (rcChannelCmd[autoCh] > autoRcThresh) {
                bAutonomous = 1;
            } else {
                bAutonomous = 0;
            }

            if (bAutonomous) {
                speedRPM = autonomousSpeed;
                m.setDirection(autoDir);
                if (autoTime > timeToPan) {
                    panSpeed = autoPanSpeed;
                } else {
                    panSpeed = 0;
                }
                long temp = (long) looptime_usec;
                autoTime = autoTime + temp;

            } else {
                autoTime = 0;
                // get speed, scale of 1000-1500 (neg), 1500-2000 (pos)
                speedRPM = (rcChannelCmd[forwardCh] - 1500) * maxSpeed / 500;
                cout << "rcChannelCmd[2]: " << rcChannelCmd[forwardCh] << endl;
                if (speedRPM < 0) {
                    currentDir = StepperMotor::ANTICLOCKWISE;
                    m.setDirection(currentDir);
                    speedRPM = -1 * speedRPM;
                } else {
                    m.setDirection(currentDir);
                }
                // get pan speed
                panSpeed = (rcChannelCmd[panCh] - 1500);
                panSpeed = panSpeed * 255 / 300;
                if (abs(panSpeed) < 7) panSpeed = 1;
            }

            // set stepper speed
            m.setSpeed(speedRPM);


            // write pan speed
            cout << "speedRPM: " << speedRPM << ", panSpeed: " << panSpeed << endl;
//            sprintf(str,"x%dx",panSpeed);
            sprintf(str,"x%dx",panSpeed);

            write(sio_file,&str,sizeof(str) + 1);
//            if ((count = write(sio_file,&panSpeed,sizeof(panSpeed))) < 0) {
//                perror("Failed to write to /dev/ttyO4");
//            };
            
//            uint_panSpeed = (unsigned int) (panSpeed);
//            writeUInt(sio_file,uint_panSpeed);

        } else {
            if (counter == checkForRCcount - 1) {
                timeStepsToCmd = 2;
            } else {
                timeStepsToCmd = 1;
            }
            int numberOfSteps = timeStepsToCmd * speedRPM * stepsPerRev / 60 * looptime_usec/1000000;
            numberOfSteps = numberOfSteps;
            if (endStop && (currentDir == endstopDir)) {
                numberOfSteps = 0;
            }

            printf("Starting thread for %d steps, %d millisec \n",numberOfSteps,(int) (looptime_usec/1000));
            if (numberOfSteps > 0) 
            m.threadedStepForDuration(numberOfSteps,(int) (timeStepsToCmd*looptime_usec/1000)); // Send in milliseconds
//            cout << "Made it out of the thread" << endl;
        }

        clock_gettime(CLOCK_REALTIME,&endloop);
        telapsed = elapsedNanos(elapsed,endloop);
        tleft_usec = looptime_usec - (telapsed/1000);
//        printf("Main loop took %d microseconds...\n",(int)(telapsed/1000));
//        printf("Going to sleep for %d microseconds...\n",tleft_usec);
        if (tleft_usec > 0) usleep(tleft_usec);

        counter++;
    }

    for (int i=0; i<6; i++) {
        close(rc_fd[i]);
    }
    printf("Closing smoothly\n");
    m.wake();



}
