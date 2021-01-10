#include "rt.h"
#include "elevator.h"
#include <thread>
#include <iostream>
#include <regex>
using namespace std;

void e1update();
void e2update();
void algo(CProcess* elevator1_process, CProcess* elevator2_process);

// Global "local" objects to be used between e1update, e2update, and algo functions
thedata elevator1data, elevator2data;
elevator e1("elevator1");
elevator e2("elevator2");
UINT e1dest,e2dest;

/* This is the MAIN dispatcher process. It creates other processes and threads.
- Creates elevator1 process, elevator2 process, io process.
- Creates e1, e2, algo thread
*/
int main(void) {
	

	// Create 3 processes
	CProcess elevator1_process("elevator1.exe", NORMAL_PRIORITY_CLASS, PARENT_WINDOW, ACTIVE);
	CProcess elevator2_process("elevator2.exe", NORMAL_PRIORITY_CLASS, PARENT_WINDOW, ACTIVE);
	CProcess io_process("io.exe", NORMAL_PRIORITY_CLASS, PARENT_WINDOW, ACTIVE);

	// Create 3 threads
	thread t1(e1update);
	thread t2(e2update);
	thread t3(algo, &elevator1_process, &elevator2_process);

	// Wait for threads to finish
	t1.join();
	t2.join();
	t3.join();

	// Waiting for processes to terminate
	elevator1_process.WaitForProcess();
	elevator2_process.WaitForProcess();
	io_process.WaitForProcess();

	// Notify user that we end program
	console.Wait();
	MOVE_CURSOR(0, 45);
	cout << "Dispatcher Shutting Down... Press Enter to end program";
	MOVE_CURSOR(0, 50);
	console.Signal();

	// Prevent auto exit
	getchar();	
	return 0;
}

// Get elevator 1 status and puts it into local elevator 1 object
void e1update() {
	CSemaphore completion("done", 0, 2);
	// Local elevator struct called status
	thedata status;
	// While loop is until we get the signal from both elevators that they're done
	while (completion.Read() != 2) {
		e1.GetElevatorStatus_dispatcher(status);
		// Assign local elevator struct to global struct
		elevator1data = status;
	}
}

// Get elevator 2 status and puts it into local elevator 2 object
void e2update() {
	CSemaphore completion("done", 0, 2);
	// Local elevator struct called status
	thedata status;
	// While loop is until we get the signal from both elevators that they're done
	while (completion.Read() != 2) {
		e2.GetElevatorStatus_dispatcher(status);
		// Assign local elevator struct to global struct
		elevator2data = status;
	}
}

/* Checks pipeline for IO commands.
Does stuff depending on the command: inside, outside, fault, exit
	-Decides what elevator the command will be given to.
	-Uses mailbox to delegate where elevator will go to.
	-Signals elevator1/2 process to update the elevator1/2 status
 looks at status of elevators and decides which elevator is in the best position to deal with incoming up/down request
 */
void algo(CProcess* elevator1_process, CProcess* elevator2_process) {
	// Pipeline to get IO data from
	CTypedPipe <io_dispatcher_pipeline> pipe1("io_dispatcher_pipeline", 1024);

	// Magic regex
	regex inside("[1-2][0-9]");
	regex outside("[u|d][0-9]");
	regex fault("[+|-][1|2]");
	regex end("[e]{2}");
	regex passenger("[d][+|-]");
	io_dispatcher_pipeline input;
	input.first = ' ';
	input.second = ' ';
		
	while (input.first != 'e' && input.second != 'e') {
		pipe1.Read(&input);
		console.Wait();
		MOVE_CURSOR(2, 6);
		cout << "Last input: " << input.first << input.second;
		MOVE_CURSOR(18, 5);
		console.Signal();
		string s({ input.first, input.second });

		// Input destination floor
		int input_dest = input.second - '0';
		bool input_dir = (input.first == 'u') ? UP : DOWN;
		//elevator1 data
		int curr1 = (int)elevator1data.curr_floor;
		bool dir1 = elevator1data.direction;
		int dest1 = (int)elevator1data.dest;
		bool idle1 = elevator1data.idle;
		bool fault1 = elevator1data.fault;
		//elevator2 data
		int curr2 = (int)elevator2data.curr_floor;
		bool dir2 = elevator2data.direction;
		int dest2 = (int)elevator2data.dest;
		bool idle2 = elevator2data.idle;
		bool fault2 = elevator2data.fault;

		// Only after a passenger?
		if (regex_search(s, inside) ) {// - Inside: 1 or 2, 0-9 {[1-2][0-9]}
			// Elevator 1
			if (input.first == '1') {
				console.Wait();
				MOVE_CURSOR(0, 2);
				cout << "Setting Elevator1 dest floor: " << input.second;
				console.Signal();
				elevator1_process->Post(input_dest);
			}
			// Elevator 2
			else if (input.first == '2') {
				console.Wait();
				MOVE_CURSOR(0, 3);
				cout << "Setting Elevator2 dest floor: " << input.second;
				console.Signal();
				elevator2_process->Post(input_dest);
			}
		}
		else if (regex_search(s, outside)) {// - outisde u or d, 0-9 {[u|d][0-9]}
			// Case 1: Both elevators are free. Get closest to command floor
			if (fault1 || fault2) {
				// if both are fault, ignore
				if (fault1 && fault2){}
				// if one is fault post to other one
				else if (fault1){
					elevator2_process->Post(input_dest);
				}
				else{
					elevator1_process->Post(input_dest);					
				}
			}
			else if (idle1 && idle2){
				// Whether u or d does not matter. Both are idle.
				// Elevator 2 is closer
				if (abs(curr1 - input_dest) > abs(curr2 - input_dest)){
					elevator2_process->Post(input_dest);
				}
				// Elevator 1 is closer
				else if (abs(curr1 - input_dest) < abs(curr2 - input_dest)){
					elevator1_process->Post(input_dest);
				}
				// Both are equal, so just go with e1
				else {
					elevator1_process->Post(input_dest);
				}
			}
			// Case 2: One elevator idle. Dont implement power savings yet
			else if (idle1 || idle2){
				// // If NOTIDLE and dir is u, and dest > curr, then use that one
				// if (idle1 == NOTIDLE && dir1 == input_dir){
				// 	if (input_dest > curr1) {

				// 	}
				// 	else if (input_dest < curr1){

				// 	}
				// 	else if (input_dest == curr1){
						
				// 	}
				// }
				// If NOTIDLE and dir is d and dest < curr, then use that one
				// Else use idle elevator
				if (idle1 == IDLE){
					elevator1_process->Post(input_dest);
				}
				else {
					elevator2_process->Post(input_dest);
				}
			}
			// Case 3: None-idle
			// Dispatcher has local map1 /map2. Decides to put into map1/2 depending on least amount of destinations to go, and closest to dest
			// if a dest matches curr for either map, tell it to stop and open.
			// 1: check queue
			// 2: map 
			// 3: braindead - assign closest to dest 
			// Faulted elevators will never come here
			else {
				
				// Assign to elevator 2
				if(abs(curr1-dest1) > abs(curr2-dest2)){
					elevator2_process->Post(input_dest);
				}
				// Otherwise assign to elevator 1
				else{
					elevator1_process->Post(input_dest);
				}
			}

			// not brainded
			/*
			else{
				if input_dir = up
					if elev1dir!=elev2dir
						if elev1dir = up && e1curr<dest  
							send e1
						if elev2dir = up && e1curr<dest
							send e2
					else
						send closer
				else inputdir = down
					if elev1dir != elev2dir
						if elev1dir = down && e1curr>dest
							send e1
						if elev2dir = down && e1curr>dest
							send e2
					else
						send closer			
			*/
		}
		// Working fault mechanism
		else if (regex_search(s, fault)) {
			//pause
			if (input.second == '1') {
				if (input.first == '-') {
					// Stop elevator 1 and clear destinations.
					elevator1_process->Post(DEST_FAULT);
				}
				if (input.first == '+') {
					// Stop elevator 1 and clear destinations.
					elevator1_process->Post(DEST_NOFAULT);
				}
			}
			else if (input.second == '2') {
				if (input.first == '-') {
					// Stop elevator 2 and clear destinations.
					elevator2_process->Post(DEST_FAULT);
				}
				if (input.first == '+') {
					// Stop elevator 2 and clear destinations.
					elevator2_process->Post(DEST_NOFAULT);
				}
			}
		}
		// Put both elevators to ground floor using GOD POSTING
		else if (regex_search(s, end)) {
			elevator1_process->Post(WHAT_IF_YOU_WANTED_TO_GO_TO_HEAVEN);
			elevator2_process->Post(BUT_GOD_SAID_UR_ELEVATOR_SUCKS);
		}
		// Should never go here because we validate input.
	}
	console.Wait();
	MOVE_CURSOR(0, 10);
	cout << "Exit triggered due to input 'ee'";
	MOVE_CURSOR(18, 5);
	console.Signal();

	return;
}

		/* insert algo here
			1. check pipeline for stuff
			2. if there is stuff, and its not ee, decide which elevator will pass to
			- if ee, go to ground, open, stop


			CHECKS:

			1. idle check
			-BOTH ELEVATORS IDLE
				Get the closest one to the command floor
				Same floor, command elevator 1

				Checks:
				1. not same curr floor, if false- send e1
				2. Closest floor (curr floor vs floor to dispatch to)

			-ONE ELEVATOR IDLE
				if passenger direction == elevator direction, stop otw, EVEN if one elevator is idle (POWER SAVINGS), sort low to high
				Otherwise use idle elevator

				Checks:
				1. If command direction == non-idle elevator direction && non-idle elevator curr floor < idle elevator curr floor
			-NONE IDLE
				Delegate to closest dest with same direction, sort low to high
				Otherwise use first to be idle.

				Checks:
				1. if elevator in same direction and before/after dispatch floor, already moving changes dest to this floor and then sets dest again
				2. if none in same direction, checks for closest to be finihsing, and puts it into that one, then sorts (vector sort)

			1
			0
			2->9
			7->9   1 0

			Overide:
			-FAULT
				Stop elevator. have graphic, clear dest vector
			-END
				Stop both elevators, go to ground, open, stop


			Pipeline io pushes to queue
			Dispatcher algo pops queue depending on idle elevators and checks above.


		*/