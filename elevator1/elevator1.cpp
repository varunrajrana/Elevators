#include "../rt.h"
#include "../elevator.h"
#include <iostream>
#include <stdlib.h>
#include <queue>

using namespace std;

/* Elevator 1:
- Have wait semaphore triggered by dispatcher
- Have mailbox to get status from dispatcher
- Update status using Update_Status()
*/
int main()
{
	// Completion semaphore to make elevator go to ground and open, then exit
	CSemaphore completion("done", 0, 2);
	CSemaphore elevator1("111", 0, 1);
	CSemaphore elevator2("222", 0, 1);

	// Named elevator object that is connected to the IO and dispatcher
	elevator e1("elevator1");

	// Mailbox object queue to get messages from
	CMailbox mailbox;

	// UINT to store message from mailbox
	UINT dest = 0;

	// Local struct to update the elevator with
	thedata update;

	// Bool is used to check for door opening
	bool doors_open = false;

	// END int for while loop check
	int end = 0;

	// For queuing up the destination to go to 
	queue<UINT> dest_queue;

	// Creating Conditions for part b for literally every floor condition
	CCondition *EV1UP[10];
	CCondition *EV1DW[10];
	for (int i = 0; i < 10; ++i) {
		EV1UP[i] = new CCondition(to_string(i) + "ev1up", AUTORESET);
		EV1DW[i] = new CCondition(to_string(i) + "ev1dw", AUTORESET);
	}

	// Do-while loop for message handling and elevator movements including up, down, door open, door close, and fault
	do {
		// Message queue
		if (mailbox.TestForMessage() && end != ITS_TIME_TO_STOP) {
			// Updates into back of queue 
			dest_queue.push(mailbox.GetMessage());

			if (dest_queue.back() == DEST_NOFAULT) {
				update.fault = NOFAULT;
				for (int i = 0; i < (int)dest_queue.size() + 1; ++i){
					dest_queue.pop();
				}
				dest_queue.push(update.curr_floor);
			}
			else if (dest_queue.back() == DEST_FAULT){
				update.fault = FAULT;
				for (int i = 0; i < (int)dest_queue.size() + 1; ++i){
					dest_queue.pop();
				}
				dest_queue.push(update.curr_floor);
			}
			else if (dest_queue.back() == ITS_TIME_TO_STOP){
				update.fault = NOFAULT;
				for (int i = 0; i < (int)dest_queue.size() + 1; ++i){
					dest_queue.pop();
				}
				dest_queue.push(0);
				dest_queue.push(ITS_TIME_TO_STOP);
			}
		}
		else if (update.fault){
			// Do nothing cause fault
		}
		else {
			// If doors are open, wait a bit for passengers to exit/enter, then close the doors.
			if (doors_open) {
				Sleep(500);
				update.doors = CLOSED;
				doors_open = false;

			}
			// If there is a destination, then the elevator is not in idle
			else if (!dest_queue.empty()){
				update.idle = NOTIDLE;
				dest = dest_queue.front();
				// If our destination is above us, we go up
				if ((int)(dest - update.curr_floor) > 0) {
					update.direction = UP;
					update.curr_floor++;
					doors_open = false;
				}
				// If our destination is below us, we go down
				else if ((int)(dest - update.curr_floor) < 0) {
					update.direction = DOWN;
					update.curr_floor--;
					doors_open = false;
				}
				// Otherwise we can assume that we're at the dest floor.
				// If our elevator doors are closed and the current floor doors are closed we can open them.
				else if (update.doors && !doors_open) {
						update.doors = OPEN;
						dest_queue.pop();
						// If our destination continues (aka size > 0) which means that we have more destinations to go to 
						if (dest_queue.size() > 0){
							update.direction = ((int)(dest_queue.front() - update.curr_floor) > 0) ? UP : DOWN;
							if (update.direction == UP){
								EV1UP[(int)update.curr_floor]->Signal();
							}
							else {
								EV1DW[(int)update.curr_floor]->Signal();
							}
						}
						// Else our direction can be up or down.
						else{
							//Signal both directions is possible
							EV1DW[(int)update.curr_floor]->Signal();
							EV1UP[(int)update.curr_floor]->Signal();
						}
						
						doors_open = true;
						// Termination check
						if (dest_queue.size() == 1)
							if (dest_queue.front() == ITS_TIME_TO_STOP)
								end = ITS_TIME_TO_STOP;		
				}
				update.dest = dest;
			}
			// If there is no destination, then the elevator is idle
			else {
				update.idle = IDLE;
			}
		}
		
		// After every message get OR fault OR elevator floor move, we update the status
		e1.Update_Status(update);
		Sleep(750);

	} while (update.curr_floor != 10 && end != ITS_TIME_TO_STOP);

	// For some reason we need to synchronize the two elevators to have a similar ending time, otherwise program doesnt terminate as some of the producer/consumer semaphores are stuck waiting.
	elevator2.Signal();
	elevator1.Wait();
	// Signal once completed
	completion.Signal();
	// Terminate elevator
	e1.ITS_TIME_TO_END();
	
	for (int i = 0; i < 10; ++i){
		delete EV1UP[i];
		delete EV1DW[i];
	}

	return 0;
}