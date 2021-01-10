#include "elevator.h"

// Constructor for elevator object class.
// Both elevator processes should construct one with a different name such that it'll be independently controlled.
elevator::elevator(string name){
	// Mutex
	theMutex = new CMutex(string("__Mutex") + name);

	// Datapool
	datapool = new CDataPool(string("__Data") + name, sizeof(struct thedata));
	datapool_ptr = (struct thedata*)(datapool->LinkDataPool()); // get pointer to data pool

	// syncrhoinzation semaphores for producer / consumer
	ps1 = new CSemaphore(string("PS1") + name, 0, 2);    // semaphore with initial value 0 and max value 1
	ps2 = new CSemaphore(string("PS2") + name, 0, 2);    // semaphore with initial value 0 and max value 1
	cs1 = new CSemaphore(string("CS1") + name, 1, 2);    // semaphore with initial value 1 and max value 1
	cs2 = new CSemaphore(string("CS2") + name, 1, 2);    // semaphore with initial value 1 and max value 1
}

// Destructor and deletes all dynamically allocated variables
elevator::~elevator(){
	delete theMutex;
	delete datapool;
	delete ps1;
	delete ps2;
	delete cs1;
	delete cs2;
}

// Termination function; to be called after elevators are terminated.
void elevator::ITS_TIME_TO_END(){
	// We only signal producers because they are the ones waiting for new writes. We want to end with a read.
	ps1->Signal();
	ps2->Signal();
}

// Producer consumer semaphore functions
// Waits for both io and dispatcher to finish reading, then locks data, then writes to it
// Then signals to them the OK to read
// Does not write until it knows dispatcher and io has read the last instance of write.
void elevator::Update_Status(thedata update) {
	CSemaphore completion("done", 0, 2);

	cs1->Wait();
	if (completion.Read() > 1)
		return;
	cs2->Wait();
	if (completion.Read() > 1)
		return;

	// Data lock
	theMutex->Wait();
	*datapool_ptr = update;
	theMutex->Signal();

	ps1->Signal();		
	ps2->Signal();
}

// Copies/reads data to dispatcher and signals that it has been copied/read.
// Does not copy/read until it has been updated.
void elevator::GetElevatorStatus_dispatcher(thedata &update) {
	CSemaphore completion("done", 0, 2);

	ps2->Wait();
	if (completion.Read() > 1)
		return;

	// Data lock
	theMutex->Wait();
	update = *datapool_ptr;
	theMutex->Signal();

	cs2->Signal();
}

// Copies/reads data to IO and signals that it has been copied/read.
// Does not copy/read until it has been updated.
void elevator::GetElevatorStatus_io(thedata &update) {
	CSemaphore completion("done", 0, 2);

	ps1->Wait();
	if (completion.Read() > 1)
		return;

	// Data lock
	theMutex->Wait();
	update = *datapool_ptr;
	theMutex->Signal();

	cs1->Signal();
}

passenger::passenger(int passnumin) {
	passnum = passnumin;
	
	srand((unsigned)time(0));
	// Get random number from 0-9 for request floor
	request_floor = (UINT)(rand() % 10);
	// Get random number from 0-9 for dest floor
	dest_floor = (UINT)(rand() % 10);

	// Must make them not to be equal 
	if (dest_floor == request_floor){
		if (dest_floor == 9)
			dest_floor--;
		else
			dest_floor++;
	}

	// Get direction from passenger's request floor - passengers dest floor
	dir = (request_floor < dest_floor) ? UP : DOWN;

	for (int i = 0; i < 10; ++i) {
		EV1UP[i] = new CCondition(to_string(i) + "ev1up");
		EV1DW[i] = new CCondition(to_string(i) + "ev1dw");
		EV2UP[i] = new CCondition(to_string(i) + "ev2up");
		EV2DW[i] = new CCondition(to_string(i) + "ev2dw");
	}
}

// Delete everything
passenger:: ~passenger() {

}

int passenger::main() {
	// First make a [U/D][0-9] command
	CTypedPipe <io_dispatcher_pipeline> pipe2("input", 1024);
	console.Wait();
	MOVE_CURSOR(0, 20+passnum);
	cout << "P" << passnum << "R: " << request_floor;
	console.Signal();

	if (dir == UP)
		temp.first = 'u';
	else
		temp.first = 'd';
	temp.second = '0' + request_floor;

	only_one.Wait();
	pipe2.Write(&temp);
	only_one.Signal();
	
	
	// Wait for elevator to arrive signal using ccondition
	temp.first = 'x';
	while (temp.first == 'x'){	
		Sleep(200);
		if (dir == UP){
			if (EV2UP[(int)request_floor]->Test()) {
				temp.first = '2';
			}
			else if (EV1UP[(int)request_floor]->Test()) { //Works
				temp.first = '1';
			}
			else
				temp.first = 'x';
		}
		else{
			if (EV2DW[(int)request_floor]->Test())
				temp.first = '2';
			else if (EV1DW[(int)request_floor]->Test())
				temp.first = '1';
			else
				temp.first = 'x';
		}
	}
	// Reset the ones that wasn't used if both directions were signalled
	if (EV1UP[(int)request_floor]->Test()){}
	if (EV1DW[(int)request_floor]->Test()){}
	if (EV2DW[(int)request_floor]->Test()){}
	if (EV2UP[(int)request_floor]->Test()){}
	// Second make a [1/2][0-9] command
	temp.second = '0' + dest_floor;
	only_one.Wait();
	pipe2.Write(&temp);
	only_one.Signal();

	console.Wait();
	MOVE_CURSOR(8, 20+passnum);
	cout << "D: " << dest_floor << " E" << temp.first;
	console.Signal();

	if (temp.first == '1') {
		if (dir == UP) {
			while (!EV1UP[(int)dest_floor]->Test()) {
				Sleep(200);
			}
		}
		else {
			while (!EV1DW[(int)dest_floor]->Test()) {
				Sleep(200);
			}
		}
	}
	else {
		if (dir == UP){
			while(!EV2UP[(int)dest_floor]->Test()){
				Sleep(200);
			}
		}
		else{
			while(!EV2DW[(int)dest_floor]->Test()){
				Sleep(200);
			}
		}
	}
	for (int i = 0; i < 10; ++i){
		delete EV1UP[i];
		delete EV2UP[i];
		delete EV1DW[i];
		delete EV2DW[i];
	}
	// Suicide
	delete this;
	return 0;
}