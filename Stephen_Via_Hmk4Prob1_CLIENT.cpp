
/*
Author: Stephen Via
Class: ECE 4122
Last Date Modified: 10/22/2019
Description:

    The purpose of this file is to create a TCP cleint on the and bind it to a locah host.
    This program should utilize multiple threads to keep the main application running while the client
    reads from the server and displays all messages using a seperate thread.

*/


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <algorithm>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <iostream>
#include <atomic>
#include <cstdio>
#include <chrono>         // std::chrono::seconds


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
void *readingServer(void* sckFd);

pthread_mutex_t mutex1; // used for print locking
pthread_mutex_t mutex2; // used for print locking

using namespace std;

struct tcpMessage   // struct of TCP MESSAGE to send between server and client
{
    unsigned char nVersion;
    unsigned char nType;
    unsigned short nMsgLen;
    char chMsg[1000];
};

// // ------------------------------------------------------------------
// // args to passs between threads
// // ------------------------------------------------------------------
struct args
{
    char* IPaddress;
    char* portNum;
};

atomic<int> CONNECTED(0); // connected to server?
atomic<int> printDone(0); // print blocking

/* This is a method used to print error when socket cannot bind.

@param *msg pointer to char array, message to print on error
*/

void error(const char *msg)
{
    perror(msg);
    exit(0);
}

/* This ia a method that is used to run a seperate thread that will listen for a response
from the server. It utilizes memset to zero out the TCPmessage and will exit proghram if
the server quits, on "q".

@param *sckFd pointer to the socketFD, cast to int
*/

void *readingServer(void *sckFd)
{

    tcpMessage recievedMsg;
    memset(&recievedMsg, 0, sizeof(recievedMsg));
    // socketFD
    int socketFd = *(int*)sckFd;
    while (CONNECTED) {

        setbuf(stdout, NULL);
        // creating the message
        int n = recv(socketFd, &recievedMsg, sizeof(recievedMsg), 0);
        if (n <= 0)  {
            CONNECTED = 0;
            sockClose(socketFd);
            sockQuit();
            exit(0);
        }
        // set to global for printing in main
        // cout << "HI THERE" << endl;
        pthread_mutex_lock(&mutex1);
        cout << flush;
        printf("\n%s\n", recievedMsg.chMsg);
        // cout << "I AM EHRRE" << endl;
        // now print in the main thread
        // unlock
        // printDone = 1;
        pthread_mutex_unlock(&mutex1);
        // waitingCond = false;
        // pthread_cond_signal(&waitingCond); // wake up main?

        // turn sending off until recieving a "t" command again
        // SENDING_BOOL = 0;
        // pthread_mutex_unlock(&mutex1);
        // exit(1);
        memset(&recievedMsg, 0, sizeof(recievedMsg));
        printf("Please enter command: ");
    }

    // close(socketFd);
    return 0;
}

int main(int argc, char *argv[])
{
    // two threads on client side:
    //      - create a thread to handle recieving messages from the server
    //      -inside of main, handle the user input (same as server side)
    // struct  hostent
    // {
    //   char    *h_name;        /* official name of host */
    //   char    **h_aliases;    /* alias list */
    //   int     h_addrtype;     /* host address type */
    //   int     h_length;       /* length of address */
    //   char    **h_addr_list;  /* list of addresses from name server */
    //   #define h_addr  h_addr_list[0]  /* address, for backward compatiblity */
    // };
    setbuf(stdout, NULL);

    if (argc < 3) {
        cout << "usage " << argv[0] << "hostanme post." << endl;
        exit(0);
    }
    // seeting lcient buffer to reset
    //
    int portNum = atoi(argv[2]);
    // creating messaing thread
    int socketFd, n;
    // memory for socket handler and length of message
    // server socket
    struct sockaddr_in serverAddr;
    // hosten structure defined in netdb.h
    struct hostent *server;
    // building the struct that contains the message
    tcpMessage newTCPMessage;
    memset(&newTCPMessage, 0, sizeof(tcpMessage));
    // socket constructor
    socketFd = socket(AF_INET, SOCK_STREAM, 0);
    if (socketFd < 0)
        error("ERROR: could not open socket.");
    // find the server
    server = gethostbyname(argv[1]);
    // error ?
    if(server == NULL)
        error("ERROR: no such host.\n");
    // zero out memory address
    bzero((char *) &serverAddr, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *) &serverAddr.sin_addr.s_addr, server->h_length);
    serverAddr.sin_port = htons(portNum);   //network byte address
    if (connect(socketFd, (struct sockaddr *) &serverAddr, sizeof(serverAddr)) < 0)
        error("ERROR connecting.\n");
    // printf("%s", inputMessage.c_str());
    // struct args *params = (struct args *)malloc(sizeof(struct args));
    // params->IPaddress = argv[1];
    // params->portNum = argv[2];
    // ----------------------------------------------------------------------
    pthread_t clientThread;
    CONNECTED = 1;
    int rc = pthread_create(&clientThread, NULL, readingServer, (void*) &socketFd); // starting the server
    rc = pthread_detach(clientThread);
    // ----------------------------------------------------------------------
    // char buffer[1000];
    // bzero(buffer, 1000);
    // ----------------------------------------------------------------------
    // parsing command input
    // ----------------------------------------------------------------------
    string command;
    string inputMessage;
    unsigned char VERSION_NO, TYPE_NO;
    // printf("Please enter command: ");
    do
    {
        // setbuf(stdout, NULL);
        // cout << flush;
        // pthread_mutex_lock(&mutex1);
        cout << flush;
        printf("Please enter command: ");
        setbuf(stdout, NULL);
        command = "";
        // pthread_mutex_unlock(&mutex1);
        // recieve
        getline(cin, command);
        cout << flush;
        // cout << command.substr(0,1) << endl;
        if (command.substr(0,2) == "v ") {
            VERSION_NO = command[2];
            // cout << VERSION_NO << endl;
            // printf("Please enter command: ");
        }
        if (command.substr(0,2) == "t ") {
            // TYPE_NO = stoi(command.substr(command.find(' '), command.find(' ', 2)));
            TYPE_NO = command[2];
            inputMessage = command.substr(command.find(' ', 2) + 1); // 3 to end of string
            // setting the version and type
            newTCPMessage.nVersion = VERSION_NO;
            newTCPMessage.nType = TYPE_NO;
            newTCPMessage.nMsgLen = (unsigned short)inputMessage.length();
            // putting into a char array
            strcpy(newTCPMessage.chMsg, inputMessage.c_str());
            // sending using socketID
            // cout << "VNO: " << VERSION_NO << endl;
            // cout << "TYPNO: " <<  TYPE_NO << endl;
            // cout << "MESSAGE: " <<  newTCPMessage.chMsg << endl;
            n = send(socketFd, &newTCPMessage, sizeof(tcpMessage), 0);
            if (n < 0)
                 error("ERROR: writing to socket.");
            // exit(1);
            // recieving comfirmation messages from the server
            // cout << inputMessage;
            // ------------------------------------------------------
            // zero out the message
            // ------------------------------------------------------
            // memset(&newTCPMessage, 0, sizeof(tcpMessage));
            // setbuf(stdout, NULL);
            // while (!printDone) { };
        }
        // cout << flush;
        // cout << command;
        // int rc = pthread_cond_wait(&waitingCond, &mutex1);
        // printDone = 0;
    } while (command[0] != 'q');
    CONNECTED = 0;
    // join the reading thread
    pthread_join(clientThread, NULL);
    // ----- DONE
    return 0;

}
