#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <limits.h>
#include <unistd.h> 
#include <string.h>
#include <sys/param.h>
#include <netdb.h>
#include <pthread.h>
#include <errno.h>

#define LEGAL_INPUT_SERVER_VARS 3
#define PORT_NUM 1
#define MIN_PORT_NUM 1
#define MAX_PORT_NUM 65535
#define MAX_FILE_SIZE 2
#define SERVER_USAGE_ERROR_MSG "Usage: srftp server-port max-file-size\n"
#define MIN_LEGAL_FILE_SIZE 0
#define MAX_PENDING_CONNECTIONS 5
#define TRUE 1

using namespace std;

int maxSize;

/**
 * This function is the receive client function. Its being initilized  with a thread which is for
 * each client crteated and gets as input the socket point to connect to.
 */ 
void* receiveClient(void* sockPoint)
{
	long fileSize;
	int fileNamelen;
	char client_message[BUFSIZ];
	int sock = *(int*)sockPoint;
	
	//first recv - file size
	int expectedSize = sizeof(long);
	int len;
	while (((expectedSize > MIN_LEGAL_FILE_SIZE) && (len = recv(sock, &fileSize, expectedSize, 0)) \
	> MIN_LEGAL_FILE_SIZE))
	{
		expectedSize -= len;
	}
	// check if recv actually actived
	if (len <= 0)
	{	
		close(sock);
		pthread_exit(0);
	}
	
	// check if filesize is valid and send it back to client
	int valid = (maxSize >= fileSize);
	write(sock, &valid, sizeof(int));
	
	// second recv - file name size
	expectedSize = sizeof(int);
	while (((expectedSize > MIN_LEGAL_FILE_SIZE) && (len = recv(sock, &fileNamelen, \
	expectedSize, 0)) > MIN_LEGAL_FILE_SIZE))
	{
		expectedSize -= len;	
	}
	// check if recv actually actived
	if (len <= 0)
	{	
		close(sock);
		pthread_exit (0);
	}
		
	// third recv - file name
	char* newFileName = (char*)malloc(sizeof(char) * fileNamelen + 1);
	expectedSize = fileNamelen + 1; 
	while (((expectedSize > MIN_LEGAL_FILE_SIZE) && (len = recv(sock, newFileName, expectedSize, 0)\
	) > MIN_LEGAL_FILE_SIZE))
	{
		expectedSize -= len;	
	}
	if ( len <= 0 )
	{	
		close(sock);
		pthread_exit (0);
	}
	
	// fourth recv- file data
	int remain_data = fileSize;
	FILE* newFile = fopen(newFileName, "w");
	while ((remain_data > MIN_LEGAL_FILE_SIZE) && (len = recv(sock, client_message, BUFSIZ, 0)) >\
	 MIN_LEGAL_FILE_SIZE)
	{
		remain_data -= len;
		fwrite(client_message, sizeof(char), len, newFile);
	}
	free(newFileName);
	fclose(newFile);
	pthread_exit (0);
}

int main(int argc, char **argv)
{
	// invalid num of vars given
	if (argc != LEGAL_INPUT_SERVER_VARS)
	{
		cout << SERVER_USAGE_ERROR_MSG;
		exit(EXIT_FAILURE);
	}
	// port num is not in the legal range 
	if (atoi(argv[PORT_NUM]) < MIN_PORT_NUM || atoi(argv[PORT_NUM]) > MAX_PORT_NUM)
	{
		cout << SERVER_USAGE_ERROR_MSG;
		exit(EXIT_FAILURE);
	}
	// given max file size is smaller than 0
	if (atol(argv[MAX_FILE_SIZE]) < MIN_LEGAL_FILE_SIZE)
	{
		cout << SERVER_USAGE_ERROR_MSG;
		exit(EXIT_FAILURE);
	}
	int socket_desc , client_sock , c ;
    struct sockaddr_in server , client;
    maxSize = atol(argv[MAX_FILE_SIZE]);
    //Create socket
    socket_desc = socket(AF_INET , SOCK_STREAM , 0);
    if (socket_desc == -1)
    {
       fprintf(stderr, "Error: function:socket errno:%d\n", errno);
    }
     
    //Prepare the sockaddr_in structure
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(atoi(argv[1]));
	//Bind
    if( bind(socket_desc,(struct sockaddr *)&server , sizeof(server)) < 0)
    {
        //print the error message
        fprintf(stderr, "Error: function:bind errno:%d\n", errno);
        exit(EXIT_FAILURE);
    }
    //Listen
    listen(socket_desc , MAX_PENDING_CONNECTIONS);
    while (TRUE)
    { 
	    //Accept and incoming connection
	    c = sizeof(struct sockaddr_in);
	     
	    //accept connection from an incoming client
	    client_sock = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c);
	    if (client_sock < 0)
	    {
	        fprintf(stderr, "Error: function:accept errno:%d\n", errno);
	        exit(EXIT_FAILURE);
	    }
	    //Receive a message from client
		pthread_t thread ;
		int res = pthread_create(&thread, NULL,  receiveClient, (void*)&client_sock);
	    if (res != 0)
	    {
			fprintf(stderr, "Error: function:pthread_create errno:%d\n", errno);
			exit(EXIT_FAILURE);
		}
	}
	
	close(socket_desc);
	return EXIT_SUCCESS;
}
