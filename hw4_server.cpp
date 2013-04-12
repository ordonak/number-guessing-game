//Author: Kenneth Ordona
//Name: hw4_server.cpp
//Class: CPSC 341
//Description: This .cpp file acts as a server to the hw4_client.cpp file.
//Basically it takes in the user's guesses and checks them against the actual
//numbers generated(randomly) by the server. Depending on the user's input, the game 
//will continue on or it will exit, stating that the user has won. A leaderboard
//will then be output to the console.

//ASSUMPTIONS: USE OF ABORT & EXIT BOOLS IS TO KEEP THE SERVER ROBUST. BASICALLY,
//IF THE USER IS TO LEAVE PREMATURELY, THE THREAD WILL NO LONGER ALLOW ANY RECEIVING
//OF STRINGS/LONGS BY DOING CHECKS ON THE EXIT BOOL. IF THE USER HAS EXITED, THE
//SYSTEM WILL NOT ALLOW ANY MORE THINGS TO BE SENT BETWEEN THE CLIENT AND SERVER

#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <iostream> 
#include <string>
#include <cmath>
#include <vector>
#include <sys/types.h> 
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <arpa/inet.h> 
#include <pthread.h>
#include <sstream>
#include <time.h>
using namespace std;


const int MAX_ARGS = 2;		//const used to hold max number of args passed
const int MAX_NUM = 3;		//const used to hold the max # of guesses & max people on leaderBoard
const int PORT_ARG = 1;		//const used to hold index of port
const int MAX_PENDING = 5;	//const used to hold max number of pending incoming requests
const int MAXPORT = 11899;	//const int used to hold max port #
const int MINPORT = 11800;	//const int used to hold min port #

//struct that holds connection info as well as roundCount+name
struct arg_t{
	int sock;
	int roundCount;
	string name;
};
//roundResult (used to judge guess)
struct roundResult{
	int tooHigh;
	int tooLow;
	int equal;
};
//leaderboard that is contained within 2 vectors, names & rCounts
struct leaderBoard{
	vector<string> names;
	vector<long> rCounts;
};
//board functions(initialize, check, insert,send and output)
void initBoard();
void sendBoard(leaderBoard lBoard, arg_t connInfo);
void checkBoard(string name, long round);
void insertBoard(int index, string name, long rounds);
void outputBoard(leaderBoard board);

//receive and send functions for longs
long receiveLong(arg_t connInfo, bool &abort);
void sendLong(long num, arg_t connInfo);

//send result function for server
void sendResult(roundResult result, arg_t connInfo);

//send and receive functions for strings
string recvString(arg_t connInfo, bool &abort);
void sendString(string msg, arg_t connInfo);

//networkOrder conversion functions
roundResult notNet(roundResult toConv);
roundResult toNet(roundResult toConvert);

//initializing function for global lock
void initLock();

//global variables(leaderboard and leaderboard lock)
leaderBoard leadBoard;
pthread_mutex_t boardLock;

void* func(void* args_pa)
{
	//reclaiming variables from args_pa
	arg_t *args_p;
	args_p = (arg_t*)args_pa;

	//setting initial variables
	srand(time(NULL));	//seeding random variable
	args_p->roundCount = 0;	//setting roundCount to 0
	string name;	//string used to hold name
	long roundCount = 0;	//long used to keepTrack of rounds
	long actualNums[MAX_NUM];	//long arr used to hold random #'s generated
	long numsGuess[MAX_NUM];	//long arr used to hold user's guess
	long numHigh, numOn, numLow;	//comparison longs(for judging game)
	bool won = false;		//bool used to test if the client has won
	roundResult result;		//uninitialized result
	roundResult *rPointer;	//result Pointer
	string victoryMess = "Congratulations! You have won "; //string used to hold victory message
	bool exit = false;
	//setting the pthread to reclaim resources once thread is exited
	pthread_detach(pthread_self());

	//receiving name from client & setting it in args_p

	name = recvString(*args_p, exit);
	//welcome message
	cerr <<  name << " has connected! " << endl;
	cerr << name << "'s " << " numbers are: ";

	//generating random numbers
	for(int i = 0; i < MAX_NUM; i++)
	{
		actualNums[i] = (rand() %201);
		cerr << actualNums[i] << "  ";
	}

	while(!exit && !won){
		args_p->name = name;
		cerr << endl << endl;

		do
		{	//receiving the guesses from client and checking them
			numHigh = 0;
			numOn = 0;
			numLow = 0;
			for(int j = 0; j < MAX_NUM; j++)
			{
				numsGuess[j] = receiveLong(*args_p, exit);
				if(!exit){
					cerr << "Received Guess(" << name<<"): " << numsGuess[j] << endl;
					cerr << "Actual Num(" << name<<"): "<< actualNums[j] << endl;
					if(numsGuess[j] < actualNums[j])
						++numLow;
					else if(numsGuess[j] > actualNums[j])
						++numHigh;
					else
						++numOn;
				}
				else{
					won = true;
					break;
				}
			}

			//setting result to the comparison longs & outputting results
			//then sending them
			result.tooHigh =numHigh;
			result.tooLow = numLow;
			result.equal = numOn;
			cerr <<"Results: " << "Too High: " << result.tooHigh << "  Too Low: " << result.tooLow << "  Equal: " << result.equal << endl;
			sendResult(result, *args_p);

			roundCount++; //round is over, increment by one
			//check to see if the user has won. If so, send victory message & 
			//set bool won to true
			if(numOn == 3){
				won = true;
				sendString(victoryMess, *args_p);
			}

		}while(!won);

		if(!exit){
			cerr << name << " has won!" << endl;

			//sending the roundCount to the client
			sendLong(roundCount, *args_p);

			//locking the checkBoard function so that only 1 client can check it at a time
			pthread_mutex_lock(&boardLock);
			checkBoard(name, roundCount);
			pthread_mutex_unlock(&boardLock);

			//outputting and sending leaderBoards
			cerr << "Current LeaderBoard: "<< endl;
			outputBoard(leadBoard);
			sendBoard(leadBoard, *args_p);
		}
		
	}
	if(exit){
		cerr << endl << "User has left prematurely! " << endl;
	}

	cerr << endl << "Now awaiting a new client!" << endl;
	//closing sockets
	close(args_p->sock);
	//exiting out of pThreads
	pthread_exit(NULL);
}

int main(int argc, char* argv[])
{	
	//checking to see if argc is correct
	if(argc != MAX_ARGS)
	{
		cerr << "Invalid number of arguments. Please input IP address for first arg, then port # for "
			<< " second one. Now exiting program.";
		exit(-1);
	}
	//assigning portNumber & checking if it is alright
	unsigned short portNum = (unsigned short)strtoul(argv[PORT_ARG], NULL, 0);
	if(portNum > MAXPORT || portNum < MINPORT)
	{
		cerr << "This port is not assigned to Ken's program. Please try again with " << endl
			<<"numbers that are between 11800 & 11899. Now exiting. ";

		exit(-1);
	}
	int status;			//status int used to check TCP functions	
	int clientSock;		//socket used to hold client

	//creating socket
	int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock < 0) {
		cerr << "Error with socket. Now exiting program. " << endl;
		close(sock);
		exit (-1);
	}

	//initializing leaderBoard & leadboardLock
	initBoard();
	initLock();

	//setting the port
	struct sockaddr_in servAddr;
	servAddr.sin_family = AF_INET; // always AF_INET
	servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servAddr.sin_port = htons(portNum);

	//binding the sockAdress & checking if it worked
	status = bind(sock, (struct sockaddr *) &servAddr,
		sizeof(servAddr));
	if (status < 0) {
		cerr << "Error with bind. Now exiting program. " << endl;
		close(sock);
		exit (-1);
	}

	//setting the server to listen for a client
	status = listen(sock, MAX_PENDING);
	cerr << "Now listening for a new client to connect to server!" << endl;
	if (status < 0) {
		cerr << "Error with listen. Now exiting program. " << endl;
		close(sock);
		exit (-1);
	}

	while(true){
		//creating tid to keep track of threads created
		pthread_t tid;

		//accepting the next client & testing if there are errors
		struct sockaddr_in clientAddr;
		socklen_t addrLen = sizeof(clientAddr);
		clientSock = accept(sock,(struct sockaddr *) &clientAddr, &addrLen);
		if (clientSock < 0) {
			cerr << "Error with accept. Now exiting program. " << endl;
			close(clientSock);
			exit(-1);
		}
		//setting the connectionInfo in args_p to send off when creating a thread
		arg_t *args_p = new arg_t;
		args_p->sock = clientSock;

		//creating threads and checking if there was an error
		status = pthread_create(&tid, NULL, func, (void*)args_p);
		if(status){
			cerr<<"Error creating threads, return code is "<< status << ". Now exiting " <<endl;
			close(clientSock);
			exit(-1);
		}
		cerr << "Client thread started." << endl;
	}
}

roundResult toNet(roundResult toConvert)
{
	toConvert.tooHigh = htonl(toConvert.tooHigh);
	toConvert.tooLow = htonl(toConvert.tooLow);
	toConvert.equal = htonl(toConvert.equal);

	return toConvert;
}

roundResult notNet(roundResult toConv)
{
	toConv.tooHigh = ntohl(toConv.tooHigh);
	toConv.tooLow = ntohl(toConv.tooLow);
	toConv.equal = ntohl(toConv.equal);

	return toConv;
}

void sendLong(long num, arg_t connInfo)
{
	long temp = htonl(num);
	int bytesSent = send(connInfo.sock, (void *) &temp, sizeof(long), 0);
	if (bytesSent != sizeof(long)) {
		cerr << "Error sending long! Now exiting. ";
		close(connInfo.sock);
		exit(-1);
	}
}

long receiveLong(arg_t connInfo, bool& abort)
{
	int bytesLeft = sizeof(long);
	long networkInt;	
	char *bp = (char*) &networkInt;

	while(bytesLeft > 0)
	{
		int bytesRecv = recv(connInfo.sock, (void*)bp, bytesLeft, 0);
		if(bytesRecv <= 0){
			abort = true;
			break;
		}
		else{
			bytesLeft = bytesLeft - bytesRecv;
			bp = bp + bytesRecv;
		}
	}
	if(!abort){
		networkInt = ntohl(networkInt);
		return networkInt;
	}
	else
		return 0;
}

void sendResult(roundResult result, arg_t connInfo)
{
	result = toNet(result);
	roundResult* rPointer;
	rPointer = &result;
	int bytesSent = send(connInfo.sock, (void *) rPointer, sizeof(result), 0);
	if (bytesSent != sizeof(result))
	{
		cerr << "Error sending results! Now exiting program.";
		close(connInfo.sock);
		exit(-1);
	}
}

void sendString(string msg, arg_t connInfo)
{
	long msgSize = (msg.length() + 1);
	char msgSend[msgSize];
	strcpy(msgSend, msg.c_str());

	sendLong(msgSize, connInfo);
	int bytesSent =  send(connInfo.sock, (void *)msgSend, msgSize, 0);
	if (bytesSent != msgSize){
		close(connInfo.sock);
		exit(-1);

	}
}

string recvString(arg_t connInfo, bool& abort)
{
	int bytesLeft = receiveLong(connInfo, abort);
	if(!abort){
		char msg[bytesLeft];
		char *bp = (char*)&msg;
		string temp;
		while(bytesLeft > 0)
		{	
			int bytesRecv = recv(connInfo.sock, (void*)bp, bytesLeft, 0);
			if(bytesRecv <= 0){
				abort = true;
				break;
			}
			else{
				bytesLeft = bytesLeft - bytesRecv;
				bp = bp + bytesRecv;
			}
		}
		if(!abort){
			temp = string(msg);
			return temp;
		}
	}
	else{
		return "--";
	}
}

void initBoard()
{
	for(int i = 0; i < MAX_NUM; i++)
	{
		leadBoard.rCounts.push_back(0);
		leadBoard.names.push_back("---");
	}
}

void checkBoard(string name, long round){
	int indexToPass = 0;
	bool leader = false;

	for(int i = (MAX_NUM-1); i >= 0; i--)
	{
		if(leadBoard.rCounts[i] > round || leadBoard.rCounts[i] == 0)
		{
			leader = true;
			indexToPass = i;
		}
	}
	if(leader)
		insertBoard(indexToPass, name, round);
}

void insertBoard(int index, string name, long rounds)
{
	if(index == (MAX_NUM-1))
	{
		leadBoard.rCounts[index] = rounds;
		leadBoard.names[index] = name;
	}

	else{
		int roundTemp = rounds;
		string nameTemp = name;

		for(int i = MAX_NUM-1; i >index; i--)
		{
			leadBoard.rCounts[i] = leadBoard.rCounts[i-1];
			leadBoard.names[i] = leadBoard.names[i-1];
		}
		leadBoard.rCounts[index] = rounds;
		leadBoard.names[index] = name;
	}
}

void sendBoard(leaderBoard lBoard, arg_t connInfo)
{
	for(int i = 0; i < MAX_NUM; i++){
		sendLong(lBoard.rCounts[i], connInfo);
		sendString(lBoard.names[i], connInfo);
	}
}

void initLock()
{
	pthread_mutex_init(&boardLock, NULL);
}

void outputBoard(leaderBoard board)
{
	cerr << endl << endl;
	cerr << "LeaderBoard" << endl << "-----------" << endl;
	for(int i = 0; i < MAX_NUM; i++)
	{
		if(board.rCounts[i] != 0)
			cerr << board.names[i] <<  "     " << board.rCounts[i] << endl;
	}
}
