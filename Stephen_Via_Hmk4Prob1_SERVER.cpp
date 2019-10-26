/*
Author: Stephen Via
Class: ECE 4122
Last Date Modified: 10/22/2019
Description:

    The purpose of this file is to create a TCP server on the local host to
    be used to recieve messages from several clients on a specified port. This program
    should utilize multiple threads to keep the main application running while the server
    accepts and keeps track of the number of clients connected to its port.

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <algorithm>
#include <unistd.h>
#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <vector>
#include <unordered_map>
#include <utility>      // std::pair, std::make_pair
#include <atomic>
#include <sstream>
#include <iomanip>

#ifdef _WIN32
   /* See http://stackoverflow.com/questions/12765743/getaddrinfo-on-win32 */
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501  /* Windows XP. */
#endif
#include <winsock2.h>
#include <Ws2tcpip.h>
#include <ws2tcpip.h>

#pragma comment (lib, "Ws2_32.lib")
#else
   /* Assume that any non-Windows platform uses POSIX-style sockets instead. */
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>  /* Needed for getaddrinfo() and freeaddrinfo() */
#include <unistd.h> /* Needed for close() */

typedef int SOCKET;
#endif

int sockInit(void)
{
#ifdef _WIN32
    WSADATA wsa_data;
    return WSAStartup(MAKEWORD(1, 1), &wsa_data);
#else
    return 0;
#endif
}

int sockQuit(void)
{
#ifdef _WIN32
    return WSACleanup();
#else
    return 0;
#endif
}

/* Note: For POSIX, typedef SOCKET as an int. */

int sockClose(SOCKET sock)
{

    int status = 0;

#ifdef _WIN32
    status = shutdown(sock, SD_BOTH);
    if (status == 0)
    {
        status = closesocket(sock);
    }
#else
    status = shutdown(sock, SHUT_RDWR);
    if (status == 0)
    {
        status = close(sock);
    }
#endif

    return status;

}


void error(const char *msg);
void *tcpServerStart(void* ptr);
void *recievingMessages(void* params);

using namespace std;

pthread_mutex_t mutex1;
pthread_mutex_t mutex2;
pthread_mutex_t mutex3;

struct tcpMessage   // struct of messages to pass between client and server
{
    unsigned char nVersion;
    unsigned char nType;
    unsigned short nMsgLen;
    char chMsg[1000];
};

// ------------------------------------------------------------------
// args to passs between threads
// ------------------------------------------------------------------
struct args {
    int newSocketFd;
    sockaddr_in clientAddr;
};
// ------------------------------------------------------------------
// ------MAP STRUCUTRE ------
// key   -- socketD, an integer
// value -- a pair of the IP and portNo
// -------------------
// connectionsMap[socketFd] = < ipAddress, portNumber >
// ------ PAIR -------
// first  -> IP address
// second -> Port Number
// -------------------
unordered_map<int, sockaddr_in> connectionsMap;
string recentMsg = "";
atomic<int> SERVER_ON(0);
vector<pthread_t> messagingThreads;
/*
This is a simple function that produces an error message to the command line prompt
if a certain condition is not met.

@param *msg a pointer to a character array
*/
void error(const char *msg)
{
    perror(msg);
    exit(0);
}


/* This is a listening thread that will start the server by binding to a socketID and
listening on that host socket. The OS will assign port numbers to accepted clients based
on a first come first serve basis. A while loop is used to create child threads for each client
to send to them. Seperate threads will be added to a messaging vector and seperate clients are added
to an unordered map with the key being the socketFD and the value being the sockaddr_in structure that
holds the port number and the IPaddress.


@param *ptr pointer the the portNum to be dereferenced as an INT.
*/
void *tcpServerStart(void* ptr)
{

    int portNum = atoi((char *)ptr);            // read port number
    // int portNum = (int *) ptr;
    // reserve memory for socket handler for server socket, port number, and message read
    int socketFd, newSocketFd;
    // struct for the server socket and the client address
    struct sockaddr_in serverAddr, clientAddr;
    // length of client
    socklen_t clientLength;
    // buffer to recieved by the server
    // tcpMessage newTCPMessage;

    // ----------------------------------------------------------------
    // int socket(int domain, int type, int protocol);
    socketFd = socket(AF_INET, SOCK_STREAM, 0);
    // Make sure socket was created
    if (socketFd < 0)
        error("ERROR: could not open socket");

    // The function bzero() sets all values in a buffer to zero.
    // It takes two arguments, the first is a pointer to the buffer and
    // the second is the size of the buffer. Thus, this line initializes serv_addr to zeros. ----
    bzero((char *) &serverAddr, sizeof(serverAddr));

    // ----------------------------------------------------------------
    // Assigning values to serverAddr socket structure
    // ----------------------------------------------------------------
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;    // unsigned long s_addr
    // For server code -> this is always the IP address of the machine which the server is running
    serverAddr.sin_port = htons(portNum);        // conver this to "network byte order"

    // ----------------------------------------------------------------
    // Binding socket handler to  binds a socket to an address,
    // in this case the address of the current host and port number on which the server will run
    // ----------------------------------------------------------------
    if (bind(socketFd, (struct sockaddr *) &serverAddr, sizeof(serverAddr)) < 0)
        error("ERROR: could not bind.");
        // The second argument is a pointer to a structure of type sockaddr, but what
        // is passed in is a structure of type sockaddr_in, and so this must be cast to
        // the correct type.

    // ----------------------------------------------------------------
    // Listening . . .
    // The first argument is the socket file descriptor, and the second is the
    // size of the backlog queue, i.e., the number of connections that can be waiting
    // while the process is handling a particular connection. This should be set to 5, the
    // maximum size permitted by most systems.
    // ----------------------------------------------------------------
    listen(socketFd, 5);
    // ----------------------------------------------------------------
    // The accept() system call causes the process to block until a client connects to the server.
    // Thus, it wakes up the process when a connection from a client has been successfully established.
    // It returns a new file descriptor, and all communication on this connection should be done using
    // the new file descriptor.
    // ----------------------------------------------------------------
    while (SERVER_ON) {
        // lock the mutex
        // pthread_mutex_lock(&mutex1);
        // accept new conncetions -> make a new thread
        clientLength = sizeof(clientAddr);
        // zero out the clientAddr memory space????
        memset(&clientAddr, 0, sizeof(clientAddr));
        // blocking ???
        newSocketFd = accept(socketFd, (struct sockaddr *) &clientAddr, &clientLength); // returns a socket ID
        // this is where the thread is blocked until accepted
        // socket that connected, handle communication between the server adn the client
        // push onto connections map
        connectionsMap[newSocketFd] = clientAddr;
        // creating a new messaging thread
        pthread_t newMsgThread;
        //packign the parameters to pass into thread creation
        struct args *params = (struct args *)malloc(sizeof(struct args));
    	params->newSocketFd = newSocketFd;
    	params->clientAddr = clientAddr;

        // new thread for recieving messages from the client
        int rc = pthread_create(&newMsgThread, NULL, recievingMessages, (void *) params); // creating thread for new client

        // push the new thread onto vector
        messagingThreads.push_back(newMsgThread);
        // unlock
        // pthread_mutex_unlock(&mutex1);
    }

    // join the client threads
    for (int i = 0; i < messagingThreads.size(); ++i) {
        pthread_join(messagingThreads[i], NULL);
    }

    // close(socketFd); // closing
}

/* This is a method to be used to spawn mutliple child threads that will recieve messages from each
of the connected clients. If a client disconnects, it should be removed from the hash map and the command
'1' should show that accordingly. This utilizes a string stream to build the messages to pass to each of the
clients, only accepting messages that have a version No. of 1. Type 0's are send to all other clients, whereas
type 1's are reversed and sent back to the respective client that passed the message to the server. A while loop
is used to tell the thread to continue operation until the "q" command is pressed in the main thread.

@param params  ptr to the newSocketFD to be used to recieve messages from.
*/
void *recievingMessages(void* params) {

    int newSocketFd = ((struct args*)params)->newSocketFd;
    // unpack the structure
    while (SERVER_ON) {
        // sockaddr_in clientAddr = ((struct args*)params)->clientAddr;
        // int portNum = ((struct args*)params)->portNum;

        // for the pass by reference recieve
        // socklen_t clientLength = sizeof(clientAddr);

        // build the new message
        tcpMessage newTCPMessage;
        memset(&newTCPMessage, 0, sizeof(tcpMessage)); // zero out memory

        int n = recv(newSocketFd, &newTCPMessage, sizeof(tcpMessage), 0);
        // printf("%s", newTCPMessage.chMsg);
        if (n <= 0) {
            // error("ERROR: could not read from socket.");
            connectionsMap.erase(newSocketFd);
            // get rid of the connection
        }

        // STRING STREAM ---- building the message to send back to client/clients
        ostringstream s1;       // building the string using string stream
        s1 << "Received Msg Type: " << newTCPMessage.nType << "; Msg: ";
        string recievedMsg = s1.str();

        // check for types  ---- version number and types
        // cout << "EHRHEHERHRE" << endl;
        tcpMessage sendingStruct;
        // if nVersion == 1
        // cout << newTCPMessage.nType << endl;
        if (newTCPMessage.nVersion == '1') {
            // save the most recent message
            recentMsg = newTCPMessage.chMsg;
            // 0 -> send to all other except current socketFD
            // 1 -> send back tp current reversed
            if (newTCPMessage.nType == '0') {
                // need to skip client that just recieved

                // build the message to output to the client
                if (connectionsMap.size() == 1) {
                    continue;
                }
                recievedMsg.append(newTCPMessage.chMsg);
                // data field of the struct
                // The c_str() function is used to return a pointer to an array that
                //    contains a null terminated sequence of character representing the
                //    current value of the string.
                strcpy(sendingStruct.chMsg, recievedMsg.c_str());
                for (auto i = connectionsMap.begin(); i != connectionsMap.end(); i++) {
                    // skip the cleint that has the same socketID
                    if (i->first == newSocketFd)
                        continue;
                    // sending the message to all except current
                    n = send(i->first, &sendingStruct, sizeof(tcpMessage), 0);
                    if (n < 0)
                        error("ERROR writing to CLIENT socket");
                }
            } else if (newTCPMessage.nType == '1') {
                string reversedMsg = newTCPMessage.chMsg;
                reverse(reversedMsg.begin(), reversedMsg.end());

                // build the message to send back to client
                recievedMsg.append(reversedMsg);
                // data field of the sturct
                strcpy(sendingStruct.chMsg, recievedMsg.c_str());
                // sending reversed to client
                n = send(newSocketFd, &sendingStruct, sizeof(tcpMessage), 0);
                if (n < 0)
                    error("ERROR writing to socket");
            }
        }
        // pthread_mutex_unlock(&mutex3);

        // sending to the client -> i got your message !!
        if (n < 0)
            error("ERROR: could not write to client socketFd");

            // close the sockets ???
    }
}

int main(int argc, char*argv[])
{
    /*----------------------------------------------------
    struct sockaddr_in
    {
      short   sin_family;       // must be AF_INET
      u_short sin_port;         // port on comp
      struct  in_addr sin_addr; // available IPV_4 address
      char    sin_zero[8];      // Not used, must be zero
    };
    ----------------------------------------------------*/

    // not enough command args
    if (argc < 2) {
        cout << "ERROR: no port provided\n" << endl;
        exit(1);
    }

    // 1. create listening socket -> thread for listening, put into listening mode.
    // 2. inside of do while -> accept returns a socketID, this thread will block until the client connects
    //      returns a socketID -> represents socket created,
    // 3. as soon as you get accept, pass socketID, thread to recieve messages
        // main thread, listening thread, n number of client thraeds that are listening
    // ----------------------------------------------------------------
    // creating a thread for the new clients listening thread
    pthread_t serverThread;
    SERVER_ON = 1;
    int rc = pthread_create(&serverThread, NULL, tcpServerStart, (void *) argv[1]); // starting the server

    // ----------------------------------------------------------------
    // parsing inputs to give server commands
    // string command = "";
    // string command = "";
    char command[0];
    do
    {
        command[0] = ' ';
        cout << "Please enter command: ";
        cin >> command;
        if (command[0] == '0') {
            // if (recentMsg.compare("") != 0) {
                cout << "Last Message: " << recentMsg << endl;
            // }
        } else if (command[0] == '1') {
            cout << "Numer of Clients: " << connectionsMap.size() << endl;
            cout << endl;
            cout << "IP Address\tPort" << endl;
            for (auto i = connectionsMap.begin(); i != connectionsMap.end(); i++) { // iterate over the map
                // i->second[.sin_addr = IPADDRESS
                // i->second.sin_port = portNum;
                cout << endl;
                cout << (i->second).sin_addr.s_addr << "\t" << (i->second).sin_port << endl;
            }
        }
        cout << flush;
    } while (command[0] != 'q');
    SERVER_ON = 0;
    // close the sockets --------------------
    for (auto i = connectionsMap.begin(); i != connectionsMap.end(); i++) { // iterate over the map
        sockClose(i->first); // closign the sockets
    }
    sockQuit();
    // close the sockets --------------------
    return 0;
    // ----------------------------------------------------------------
}
