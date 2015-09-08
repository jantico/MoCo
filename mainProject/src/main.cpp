/* Main MoCo function 

Beaglebone is main control board.  
Arduino listens for RF commands and moves the DC motor accordingly.


By: Jason Antico
*/

// Includes and defines
#include "easyBlack/memGPIO.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdint.h>

#define BILLION 1000000000L


// Variables
struct RxPins {
	easyBlack::memGPIO::gpioPin pin[6];
	unsigned char value[6];
};

struct PanGoCmds {
	int linSpeed;
	int rotSpeed;
};

easyBlack::memGPIO bbbGPIO;

enum ModeEnum {
	manual,
	autonomous
};

// Any subfunctions

/* Setup Receiver pins
*/
void setRxPins(RxPins *rx, std::string pinNum[6]) {
	for (int i=0; i<6; i++) {
		if (!pinNum[i].empty()) {
			// get the pin structure
			rx->pin[i] = bbbGPIO.getPin(pinNum[i]);
			// Need to setup the pin still
			bbbGPIO.pinMode(rx->pin[i], easyBlack::memGPIO::INPUT);
		}
	}
}

/* Get Receiver Cmds
This function will need to be updated to read 
*/
void getRxCmds(RxPins *rx) {
	for (int i=0; i<6; i++) {
		if (!rx->pin[i].name.empty()) {
			rx->value[i] = bbbGPIO.digitalRead(rx->pin[i]);
		}
	}
}

/* elapsedNanos
Calculate time in nanosecs from two timespecs
*/
uint64_t elapsedNanos(timespec start, timespec end) {
	return BILLION * (end.tv_sec - start.tv_sec) + end.tv_nsec - start.tv_nsec;
}

/* getOpMode
Map RX Cmds to Operation mode 
Outputs Enum "manual" or "autonomous"
*/
ModeEnum getOpMode(RxPins rx) {
	int opMode;
	// check rx commands to get operation mode
	return (ModeEnum)(opMode);
}

/* mapRx2PanGo
Maps RX commands to Pan and Go commands
*/
PanGoCmds mapRx2PanGo(RxPins rx) {
	PanGoCmds pg;
	// Calculate commands from RX
	return pg;
}

/* runManualMode
Loops in manual mode until the control mode changes
*/
void runManualMode(RxPins *rx) {
	struct timespec start, last, elapsed;
	clock_gettime(CLOCK_REALTIME,&start);
	uint64_t tElapsed;
	ModeEnum opMode = manual;
	PanGoCmds desiredCmd;

	int cycleCounter = 0;
	int cycleReset = 50; // Default value, need to test

	last = start;

	// Loop until mode changes 
	while(1) {
		last = elapsed;
		clock_gettime(CLOCK_REALTIME, &elapsed);
		tElapsed = elapsedNanos(last, elapsed);
		// We only want to check the rx commands every so many cycles to save computation time
		// 50 is just a 
		if (cycleCounter > cycleReset) {
			getRxCmds(rx);
			desiredCmd = mapRx2PanGo(*rx);
			cycleCounter = 0;
			opMode = getOpMode(*rx);
		} else {
			cycleCounter++;
		}
			

		// break if switched to autonomous mode
		if (opMode == autonomous) break;

		// Send cmds to stepper

		// Send cmds via RF to Arduino


	}
}

/* runAutoMode
Loops in autonomous mode until the control mode changes
*/
void runAutoMode(RxPins *rx) {
	struct timespec start, last, elapsed;
	clock_gettime(CLOCK_REALTIME,&start);
	uint64_t tElapsed;
	ModeEnum opMode = autonomous;
	PanGoCmds desiredCmd;

	int cycleCounter = 0;
	int cycleReset = 100; // Default value, need to test

	last = start;

	uint64_t timeToPan;
	// Calculate time to pan based on autonomous requirements
	// Eventually, we should be able to save a speed from the manual settings

	// Loop until mode changes 
	while(1) {
		last = elapsed;
		clock_gettime(CLOCK_REALTIME, &elapsed);
		tElapsed = elapsedNanos(last, elapsed);

		
		// We only want to check the rx commands every so many cycles to save computation time
		if (cycleCounter > cycleReset) {
			getRxCmds(rx);
			opMode = getOpMode(*rx);
		} else {
			cycleCounter++;
		}
			
		// break if switched to autonomous mode
		if (opMode == manual) break;

		// Send cmds to stepper

		// Check if it's time to command the Arduino to pan?
		if (elapsedNanos(elapsed,start) > timeToPan) {
			// Send cmds via RF to Arduino

		}
		


	}
}


// Main routine
int main(void) {
	// local initializations
	RxPins rx;

	// Wait for wakeup from receiver
	bool wakeup = false;
	while (!wakeup) {
		getRxCmds(&rx);
		for (int i=0; i<6; i++) {
			if (!rx.pin[i].name.empty()) {
				wakeup = true;
			}
		}
	}

	// Got the wakeup!  now check for Operation mode
	ModeEnum opMode = manual;
	// If manual mode, respond to RC commands at a higher rate
	while(1) {
		// TODO: insert code to check for op mode

		if (opMode == manual){
			runManualMode(&rx);
		} else {	// Else: autonomous mode, check for switch to manual every X cycles
			runAutoMode(&rx);
		}
	}

	bbbGPIO.~memGPIO();

	exit(EXIT_SUCCESS);

}
