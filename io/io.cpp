#include "../rt.h"
#include "../elevator.h"
#include <iostream>
#include <thread>
#include <regex>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <ctime>


using namespace std;

void e1display();
void e2display();
void input();

void human_input();

void setscene();
void anielevator(int elev, int curr_floor, bool closed, bool fault, int prev);

HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

// Part B
void dynamic_passenger();

/*
Deals with all things displayed on console.
Gets user input and sends it through pipeline to Dispatcher
Get_elevator_status to show current status and animations
*/


int main()
{
	/*cout << "Hello from io process !!!!!!" << endl;*/
	setscene();

	// Creating three threads: updates from elevator1 / 2 + screen redraw, input of commands
	thread t1(e1display);
	thread t2(e2display);
	thread t3(input);
	thread t4(human_input);
	thread t5(dynamic_passenger);

	// Wait for threads to finish.
	t1.join();
	t2.join();
	t3.join();
	t4.join();
	t5.join();

	console.Wait();
	MOVE_CURSOR(0, 11);
	cout << "IO Process ended ";
	MOVE_CURSOR(18, 5);
	console.Signal();
	return 0;
}

// Handles updates from elevator 1 + redraws screen using move_cursor + mutex
// Attempts to get status of elevator. may be blocked if no new data
void e1display() {
	CSemaphore completion("done", 0, 2);
	elevator e1("elevator1");
	thedata update;
	int prev = 0;
	int statux = STATUS_X + 5;
	console.Wait();
	MOVE_CURSOR(STATUS_X,STATUS_Y);
	SetConsoleTextAttribute(hConsole,YELLOW);
	cout << R"(   --------~ELEVATOR 1~------
						|                        |
						|                        |
						|                        |
						|                        |
						|                        |
						--------------------------)";
	SetConsoleTextAttribute(hConsole, WHITE);
	console.Signal();
	while (completion.Read() != 2) {
		e1.GetElevatorStatus_io(update);
		console.Wait();
		MOVE_CURSOR(statux,STATUS_Y+1);             	// move cursor to cords [x,y] 
		cout << "Current floor: " << update.curr_floor;
		MOVE_CURSOR(statux,STATUS_Y+2);             	// move cursor to cords [x,y] 
		if (update.idle)
			cout << "Idle    ";
		else
			cout << "Not idle";
		MOVE_CURSOR(statux,STATUS_Y+3);             	// move cursor to cords [x,y] 
		if (update.doors)
			cout << "Doors Closed";
		else{
			SetConsoleTextAttribute(hConsole, BRIGHTGREEN);
			cout << "Doors Open  ";
			SetConsoleTextAttribute(hConsole, WHITE);
		}
		MOVE_CURSOR(statux,STATUS_Y+4);             	// move cursor to cords [x,y] 
		if (update.direction)
			cout << "Going up   ";
		else
			cout << "Going down";
		MOVE_CURSOR(statux,STATUS_Y+5);             	// move cursor to cords [x,y] 
		if (update.fault) {
			SetConsoleTextAttribute(hConsole, BRIGHTRED);
			for (int i = STATUS_Y+1; i < STATUS_Y+6; i++) {
				MOVE_CURSOR(statux, i);
				cout << "FAULT            ";
			}
			SetConsoleTextAttribute(hConsole, WHITE);
		}
		else
			cout << "no fault";
		cout.flush();
		anielevator(1, update.curr_floor, update.doors, update.fault, prev);
		console.Signal();
		prev = update.curr_floor;
	}
}

// Handles updates from elevator 2 + redraws screen using move_cursor + mutex
// Attempts to get status of elevator. may be blocked if no new data
void e2display() {
	CSemaphore completion("done", 0, 2);
	elevator e2("elevator2");
	thedata update;
	int prev = 0;
	int statusx = STATUS_X + 5;
	console.Wait();
	MOVE_CURSOR(STATUS_X,STATUS_Y+9);
	SetConsoleTextAttribute(hConsole, YELLOW);
	cout << R"(   -------~ELEVATOR 2~-------
						|                        |
						|                        |
						|                        |
						|                        |
						|                        |
						--------------------------)";
	SetConsoleTextAttribute(hConsole, WHITE);
	
	console.Signal();
	while (completion.Read() != 2) {
		e2.GetElevatorStatus_io(update);
		console.Wait();
		MOVE_CURSOR(statusx,STATUS_Y+10);             	// move cursor to cords [x,y] 
		cout << "Current floor: " << update.curr_floor;
		MOVE_CURSOR(statusx,STATUS_Y+11);             	// move cursor to cords [x,y] 
		if (update.idle)
			cout << "Idle    ";
		else
			cout << "Not idle";
		MOVE_CURSOR(statusx,STATUS_Y+12);             	// move cursor to cords [x,y] 
		if (update.doors)
			cout << "Doors Closed";
		else{
			SetConsoleTextAttribute(hConsole, BRIGHTGREEN);
			cout << "Doors Open  ";
			SetConsoleTextAttribute(hConsole, WHITE);
		}
		MOVE_CURSOR(statusx,STATUS_Y+13);             	// move cursor to cords [x,y] 
		if (update.direction)
			cout << "Going up  ";
		else
			cout << "Going down";
		MOVE_CURSOR(statusx,STATUS_Y+14);             	// move cursor to cords [x,y] 
		if (update.fault){
			SetConsoleTextAttribute(hConsole, BRIGHTRED);
			for (int i = STATUS_Y+10; i < STATUS_Y+15; i++) {
				MOVE_CURSOR(statusx, i);
				cout << "FAULT            ";
			}
			SetConsoleTextAttribute(hConsole, WHITE);
		}
		else
			cout << "no fault";
		cout.flush();
		anielevator(2, update.curr_floor, update.doors, update.fault, prev);
		console.Signal();
		prev = update.curr_floor;
	}
}

// Handles input using getch()
// Cool regex magic
// Pipelines validated io commands to dispatcher
void input() {
	CSemaphore completion("done", 0, 2);
	CSemaphore on("ddd", 0, 1);
	CSemaphore off("dddddd", 0, 1);
	
	CTypedPipe <io_dispatcher_pipeline> pipe1("io_dispatcher_pipeline", 1024);
	CTypedPipe <io_dispatcher_pipeline> pipe2("input", 1024);
	io_dispatcher_pipeline temp;
	io_dispatcher_pipeline input_from_dynamic_passengers;
	regex inside("[1-2][0-9]");
	regex outside("[u|d][0-9]");
	regex fault("[+|-][1|2]");
	regex end("[e]{2}");
	regex passenger("[d][+|-]");
	char fist;
	char secd;
	bool max_one_dynamic = false;

	console.Wait();
	MOVE_CURSOR(0, 4);
	cout << R"(--------------------------
| Enter characters:      |
|                        |
--------------------------)";
	console.Signal();
	// Forever loop
	while (completion.Read() != 2) {
		// Get characters
		if (completion.Read() == 2)
			return;
		// Pipeline input
		only_one.Wait();
		if (pipe2.TestForData()>0){
			pipe2.Read(&input_from_dynamic_passengers);
			only_one.Signal();
			fist = input_from_dynamic_passengers.first;
			secd = input_from_dynamic_passengers.second;
		}
		else{
			only_one.Signal();
			continue;
		}
		
		if (completion.Read() == 2)
			return;

		string s({ fist, secd });

		if (regex_search(s, inside)) {
			console.Wait();
			MOVE_CURSOR(1, 8);
			SetConsoleTextAttribute(hConsole, PURPLE);
			cout << "matched inside                           ";
			SetConsoleTextAttribute(hConsole, WHITE);
			console.Signal();
		}
		else if (regex_search(s, outside)) {
			console.Wait();
			MOVE_CURSOR(1, 8);
			SetConsoleTextAttribute(hConsole, PURPLE);
			cout << "matched outside                           ";
			SetConsoleTextAttribute(hConsole, WHITE);
			console.Signal();
		}
		else if (regex_search(s, fault)) {
			console.Wait();
			MOVE_CURSOR(1, 8);
			SetConsoleTextAttribute(hConsole, PURPLE);
			cout << "Matched fault                             ";
			SetConsoleTextAttribute(hConsole, WHITE);
			console.Signal();
		}
		else if (regex_search(s, end)) {
			console.Wait();
			MOVE_CURSOR(1, 8);
			SetConsoleTextAttribute(hConsole, PURPLE);
			cout << "Matched end                               ";
			SetConsoleTextAttribute(hConsole, WHITE);
			console.Signal();
			temp.first = fist;
			temp.second = secd;
			pipe1.Write(&temp);
			return;
		}
		else if (regex_search(s, passenger)) {
					console.Wait();
					MOVE_CURSOR(0, 19);
					//cout << "REGEX REGEX GESREXGXGEGGXGXGXGX loop";
					console.Signal();
			if (secd == '+') {
				if (max_one_dynamic == false) {
					max_one_dynamic = true;
					console.Wait();
					MOVE_CURSOR(0, 19);
					//cout << "WE IN fasasfasfasfHERE in loop";
					console.Signal();
					on.Signal();
				}
				else
					continue; 
			}
			else{
				if (max_one_dynamic == false){
					continue;
				}
				else{
					max_one_dynamic = false;
					console.Wait();
					MOVE_CURSOR(0, 19);
					//cout << "WE IN H1111111111111ERE in loop";
					console.Signal();
					off.Signal();
				}
			}
			console.Wait();
			MOVE_CURSOR(1, 8);
			SetConsoleTextAttribute(hConsole, BRIGHTCYAN);
			cout << "Matched passenger mode                    ";
			SetConsoleTextAttribute(hConsole, WHITE);
			console.Signal();
			continue;
		}
		else {
			console.Wait();
			MOVE_CURSOR(1, 8);
			SetConsoleTextAttribute(hConsole, BRIGHTRED);
			cout << "Incorrect instruction. Please try again.";
			SetConsoleTextAttribute(hConsole, WHITE);
			console.Signal();
			continue;
		}
		temp.first = fist;
		temp.second = secd;
		pipe1.Write(&temp);
	}
}

void human_input(){
	CTypedPipe <io_dispatcher_pipeline> pipe2("input", 1024);
	io_dispatcher_pipeline temp;
	CSemaphore completion("done", 0, 2);
	
	while (completion.Read() != 2){
		temp.first = _getch();
		console.Wait();
		MOVE_CURSOR(21, 5);
		cout << temp.first;
		console.Signal();
		temp.second = _getch();
		console.Wait();
		MOVE_CURSOR(22, 5);
		cout << temp.second;
		MOVE_CURSOR(21, 5);
		cout << "  ";
		console.Signal();


		if (completion.Read() == 2)
			return;
		only_one.Wait();
		pipe2.Write(&temp);		
		only_one.Signal();
	}
}

// always running thread 
void dynamic_passenger(){
	// Active object uses pipe2 mutex
	// CMutex only_one_write("pipe2");
	CSemaphore limit("limit", 0, 4);
	CSemaphore completion("done", 0, 2);
	CSemaphore on("ddd", 0, 1);
	CSemaphore off("dddddd", 0, 1);

	while (completion.Read() != 2){
		console.Wait();
		MOVE_CURSOR(0, 18);
		//cout << "WE OUT HERE in loop";
		console.Signal();
		on.Wait();
		console.Wait();
		MOVE_CURSOR(0, 19);
		//cout << "WE IN HERE in loop";
		console.Signal();
		// While off is not signalled.
		for (int i = 0; off.Read() != 1; ++i){
			// Make active object passenger
			passenger myPassenger(i);
			myPassenger.Resume();
			srand((unsigned)time(0));
			Sleep(1000 * ( rand() % 10 + 5 ) );
			// TODO: might have runtime error because no join or detach (not even present in CThread)
		}
		// Remove the 1 from off
		off.Wait(); 
	}

}

//Draws tower
void setscene() {
	console.Wait();
	MOVE_CURSOR(0, 0);
	SetConsoleTextAttribute(hConsole, BRIGHTCYAN);
	cout << R"(	     
										     _________|_________     || 
										    /                   \   _||_
										   <                     > ( oo )
										   <      Freefall       >_| ^^ |_
										   <      Elevators      >   @    \
										  /~                     ~\ . . _ |
										 /~~                     ~~\    | |
										/~~~~~~~~~~~~~~~~~~~~~~~~~~~\/ _| |
										|[            |            ]/ / [m]
										|[            |            [m]
										|[            |            ]|
										|[            |            ]|
										|[============|============]|
										|[            |            ]|
										|[            |            ]|
										|[            |            ]|
										|[            |            ]|
										|[============|============]|
										|[            |            ]|
										|[            |            ]|
										|[            |            ]|
										|[            |            ]|
										|[============|============]|
										|[            |            ]|
										|[            |            ]|
										|[            |            ]|
										|[            |            ]|
										|[============|============]|
										|[            |            ]|
										|[            |            ]|
										|[            |            ]|
										|[            |            ]|
										|[============|============]|
										|[            |            ]|
										|[            |            ]|
										|[            |            ]|
										|[            |            ]|
										|[============|============]|
										|[            |            ]|
										|[            |            ]|
										|[            |            ]|
										|[            |            ]|
										|[============|============]|
										|[            |            ]|
										|[            |            ]|
										|[            |            ]|
										|[            |            ]|
										|[============|============]|
										|[            |            ]|
										|[            |            ]|
										|[            |            ]|
										|[            |            ]|
										|[============|============]|
										|[            |            ]|
										|[            |            ]|
										|[            |            ]|
										|[            |            ]|
										[===========================])";
	// TODO: make bottom bulbous
	SetConsoleTextAttribute(hConsole, WHITE);
	console.Signal();
	return;
}

//draws elevators
void anielevator(int elev, int curr_floor, bool doors, bool fault, int prev) {
	int topleftx = 84 + ((elev - 1) * 13);
	int toplefty = 54 - (5 * curr_floor);
	int toplefty_prev = 54 - (5 * prev);
	console.Wait();

	MOVE_CURSOR(topleftx, toplefty_prev);
	cout << "         ";
	MOVE_CURSOR(topleftx, toplefty_prev + 1);
	cout << "         ";
	MOVE_CURSOR(topleftx, toplefty_prev + 2);
	cout << "         ";
	MOVE_CURSOR(topleftx, toplefty_prev + 3);
	cout << "         ";

	if (doors) {
		MOVE_CURSOR(topleftx, toplefty);
		cout << "--------";
		MOVE_CURSOR(topleftx, toplefty + 1);
		cout << "|  ||  |";
		MOVE_CURSOR(topleftx, toplefty + 2);
		cout << "|  ||  |";
		MOVE_CURSOR(topleftx, toplefty + 3);
		cout << "--------";
	}
	else if (!doors) {
		SetConsoleTextAttribute(hConsole, BRIGHTGREEN);
		MOVE_CURSOR(topleftx, toplefty);
		cout << "--------";
		MOVE_CURSOR(topleftx, toplefty + 1);
		cout << "||    ||";
		MOVE_CURSOR(topleftx, toplefty + 2);
		cout << "||    ||";
		MOVE_CURSOR(topleftx, toplefty + 3);
		cout << "--------";
		SetConsoleTextAttribute(hConsole, WHITE);
	}
	if (fault) {
		SetConsoleTextAttribute(hConsole, BRIGHTRED);
		MOVE_CURSOR(topleftx, toplefty);
		cout << "--------";
		MOVE_CURSOR(topleftx, toplefty + 1);
		cout << "|  \\/  |";
		MOVE_CURSOR(topleftx, toplefty + 2);
		cout << "|  /\\  |";
		MOVE_CURSOR(topleftx, toplefty + 3);
		cout << "--------";
		SetConsoleTextAttribute(hConsole, WHITE);
	}

	console.Signal();
	return;
}