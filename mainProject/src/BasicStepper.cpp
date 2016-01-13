
/* StepperDriver.cpp:
 * Program to read a stepper motor speed from a file (in steps/sec) and output through Beaglebone * GPIO interface.
 * Running as part of a separate process in order to write the pulse at as smooth of a speed as  * possible.

 */

#define __STDC_FORMAT_MACROS

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
#include <inttypes.h>


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


/* elapsedMicros
Calculate time in microsecs from two timespecs
*/
uint64_t elapsedMicros(timespec start, timespec end) {
    return 1000000 * (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec)/1000;
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




    // upkeep variables
    struct timespec start, last, elapsed, endloop;
    clock_gettime(CLOCK_REALTIME,&start);
    last = start;
    elapsed = last;

    uint64_t telapsed;
    float tfloat;

 
    //Setup stepper motor object
    StepperMotor m(pinMS1,pinMS2,pinStep,pinSleep,pinDir,speedRPM,stepsPerRev);
    m.sleep();

    // Calculate step speed
    clock_gettime(CLOCK_REALTIME, &start);
    m.step();
    clock_gettime(CLOCK_REALTIME, &elapsed);
    telapsed = elapsedMicros(start,elapsed);
    int elapsed_msec = (int) telapsed;
    cout << "Step time: " << elapsed_msec << endl;
    
    // input vars
    int speedIn = 0;
    int commandMode = 0;
    int timeIn = 0;

    // flags
    int changeStepperSpeed = 0;
    int changePanSpeed = 0;
    int runFlag = 1;
    int stopPan = 0;

    // pan variables
    int panSpeed;
    unsigned int uint_panSpeed;
    char str[10];
    int sio_file;
    if ((sio_file = open("/dev/ttyO4", O_RDWR | O_NOCTTY | O_NDELAY))<0) {
        perror("UART: Failed to open the file.\n");
        return -1;
    }
    struct termios options;
    tcgetattr(sio_file, &options);
    options.c_cflag = B2400 | CS8 | CLOCAL;
    options.c_iflag = IGNPAR | ICRNL;
    tcflush(sio_file, TCIFLUSH);
    tcsetattr(sio_file, TCSANOW, &options);


    int deltat = 10; // msec

    m.threadedStepAtSpeed(1); // Send in milliseconds


    while (1) {
        if (flag) {
            break;
        }
        last = elapsed;
//        clock_gettime(CLOCK_REALTIME,&elapsed);
//        telapsed = elapsedNanos(last,elapsed);
//        tfloat = (float) (telapsed/BILLION);
 //       printf("elapsed time: %f\n",tfloat);
//        telapsed = elapsedNanos(elapsed,endloop);
        telapsed = elapsedMicros(elapsed,endloop);
        printf("%" PRIu64 "\n",telapsed);
        printf("telapsed: %f \n",(double) (telapsed/1000));

        changeStepperSpeed = 0;
        changePanSpeed = 0;

        cout << "Select a command mode: \n";
        cout << "   1: set stepper speed \n";
        cout << "   2: set the pan speed \n";
        cout << "   3: pause/resume stepper \n";
        cout << "   4: pause/resume pan \n";
        cout << "   5: quit program \n";
       cin >> commandMode;
        
        switch (commandMode) 
        {
            case 1:
                cout << "Enter the speed (RPM): ";
                cin >> speedIn;
                changeStepperSpeed = 1; 
                break;
            case 2:
                cout << "Enter the pan speed (0-255): ";
                cin >> panSpeed;
                changePanSpeed = 1;
                break;
            case 3:
                //sprintf(str,"x666x");
                //write(sio_file,&str,sizeof(str)+1);
                runFlag = !runFlag;
                break;
            case 4:
                sprintf(str,"x666x");
                write(sio_file,&str,sizeof(str)+1);
                stopPan = !stopPan;
            case 5:
                return 0;
        }
//        cout << "Enter the desired duration (msec): ";
//        cin >> timeIn;

//        printf("Inputs: %d,%d\n",speedIn,timeIn);
 
        // Stepper speed requested
        if (changeStepperSpeed) {
            clock_gettime(CLOCK_REALTIME,&elapsed);
            if (speedIn < 0) {
                m.setDirection(StepperMotor::ANTICLOCKWISE);
                speedIn = -1 * speedIn;
            } else {
                m.setDirection(StepperMotor::CLOCKWISE);
            }
            m.setSpeed(speedIn);           
        }

        if (changePanSpeed) {
//            uint_panSpeed = (unsigned int) (panSpeed);
//            writeUInt(sio_file,uint_panSpeed);
            cout << "Setting pan speed:"  << panSpeed;
            sprintf(str,"x%dx",panSpeed);
            write(sio_file,&str,sizeof(str) + 1);
            write(sio_file,&str,sizeof(str) + 1);
            write(sio_file,&str,sizeof(str) + 1);
        }

        if (!runFlag) {
            m.sleep();
//            writeUInt(sio_file,0);
            sprintf(str,"x0x");
            write(sio_file,&str,sizeof(str) + 1);
        } else {
            m.wake();
//            writeUInt(sio_file,uint_panSpeed);
        }

//        int numberOfSteps = speedIn * stepsPerRev * timeIn / 60 / 1000;
//        printf("number of steps: %d\n",numberOfSteps);
//        m.threadedStepForDuration(numberOfSteps,timeIn); // Send in milliseconds
//        usleep((timeIn + deltat) * 1000);

//        printf("Speed: %d\n",speedIn);
        usleep(3000000); // Sleep 3 seconds
        clock_gettime(CLOCK_REALTIME,&endloop);
    }


    m.wake();



}
