// This header file contains the function prototypes, struct, and class declarations for the actual elevator
#ifndef __ELEVATOR__
#define __ELEVATOR__

// Includes
#include "rt.h"
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <ctime>

// Definitions for elevators so we don't need to be confused when we type 1 / 2 
#define ELEVATOR1 1
#define ELEVATOR2 2

// Definitions for text colours for some cool ~aesthetic~
#define BLACK 0
#define RED 4
#define YELLOW 6
#define GREEN 2
#define BLUE 1
#define PURPLE 5
#define WHITE 7 
#define GRAY 8
#define CYAN 3
#define BRIGHTCYAN 11
#define BRIGHTRED 12
#define BRIGHTGREEN 10
#define BRIGHTWHITE 16

// Default x and y coordinates
#define STATUS_X 45
#define STATUS_Y 4

// Making it easier to understand assignments
#define UP true
#define DOWN false
#define IDLE true
#define NOTIDLE false
#define CLOSED true
#define OPEN false
#define FAULT true
#define NOFAULT false
#define DEST_FAULT 100
#define DEST_NOFAULT 200

// Terminator values
#define WHAT_IF_YOU_WANTED_TO_GO_TO_HEAVEN 666
#define BUT_GOD_SAID_UR_ELEVATOR_SUCKS 666
#define ITS_TIME_TO_STOP 666

// "global" mutex for handling console writes
const CMutex console("M1");
// "global" mutex for handling pipeline writes 
const CMutex only_one("pipe2");



// Struct to be written to and read from the elevator
struct thedata {
	UINT curr_floor = 0; 	//Current floor is in UINT so then we don't need to do conversions between int and UINT. Somehow conversions cause it to display as ascii instead of an int, so we just left it here as UINT
	bool doors = CLOSED; 	// Bool for handling doors open or closed. True=closed / False = open
	bool idle = IDLE;  		// Bool for handling if elevator is idle or not. True = idle / False = not idle
	bool direction = UP; 	//true = up, false = down
	bool fault = NOFAULT;	// True = fault, false = nofault
	UINT dest = 0; 			// For dispatcher algorithm only
};

// IO <-> Dispatcher pipeline. Contains two chars that are already validated.
struct io_dispatcher_pipeline {
	char first;
	char second;
};

// Named elevator monitor that contains producer and consumer semaphores that synchronize get&update status commands.
class elevator{
private:

	// Empty pointers to be named during construction of this monitor
	CMutex* theMutex;
	CDataPool* datapool;

	// Datapool pointer that is composed of our data struct. Filled during the construction of this monitor
	struct thedata* datapool_ptr;
	
	// Producer and consumer semaphores
	CSemaphore* ps1;
	CSemaphore* ps2;
	CSemaphore* cs1;
	CSemaphore* cs2;

public:
	// Constructor function declaration
	elevator(string name);
	// Destructor function declaration
	~elevator();
	// IO read Synchronization function declaration
	void GetElevatorStatus_io(thedata &update);
	// Dispatcher read synchronization function declaration
	void GetElevatorStatus_dispatcher(thedata &update);
	// Write synchronization with IO and Dispatcher function declaration
	void Update_Status(thedata update);
	// Termination function declaration
	void ITS_TIME_TO_END();
};

class passenger : public ActiveClass {
private:
	//CTypedPipe <io_dispatcher_pipeline> pipe2("input", 1024);

	io_dispatcher_pipeline temp;
	UINT request_floor;
	UINT dest_floor;
	int passnum;
	bool dir;
	int main();
	// Creating Conditions for part b for literally every floor condition
	CCondition *EV1UP[10];
	CCondition *EV2UP[10];
	CCondition *EV1DW[10];
	CCondition *EV2DW[10];

public:
	passenger(int passnumin);
	~passenger();
};

#endif // !__ELEVATOR__