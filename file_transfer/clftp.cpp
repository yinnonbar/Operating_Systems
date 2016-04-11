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
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <fcntl.h>
#include <errno.h>

#define CLIENT_USAGE_ERROR_MSG "Usage: clftp server-port server-hostname file-to-transfer \
filename-in-server\n"
#define LEGAL_INPUT_CLIENT_VARS 5
#define MIN_LEGAL_TRANSFER_SIZE 0
#define NO_DATA_TRANSFER 0
#define MIN_PORT_NUM 1
#define MAX_PORT_NUM 65535
#define PORT_NUM 1
#define HOST_NAME 2
#define FILE_NAME 3
#define NEW_FILE_NAME 4
#define ILLEGAL_CONNECT_RETURN_VALUE 0
using namespace std;

int main(int argc, char **argv)
{
	int sock;
	struct hostent *he;
    struct sockaddr_in server;     
    // invalid num of vars given
    if (argc != LEGAL_INPUT_CLIENT_VARS)
	{
		cout << CLIENT_USAGE_ERROR_MSG;
		exit(EXIT_FAILURE);
	} 
    //port number is not in the legal range
    if (atoi(argv[PORT_NUM]) < MIN_PORT_NUM || atoi(argv[PORT_NUM]) > MAX_PORT_NUM)
	{
		cout << CLIENT_USAGE_ERROR_MSG;
		exit(EXIT_FAILURE);
	}
    //creates the socket
    sock = socket(AF_INET , SOCK_STREAM , 0);
    if (sock == -1)
    {
        fprintf(stderr, "Error: function:socket errno:%d\n", errno);
		exit(1);
    }
    // get the host name
    if ((he = gethostbyname(argv[HOST_NAME])) == NULL)
    {
        fprintf(stderr, "Error: function:gethostbyname errno:%d\n", errno);
        exit(EXIT_FAILURE);
    }
    
   bzero((char *) &server, sizeof(server));
   server.sin_family = AF_INET;
   bcopy((char *)he->h_addr, (char *)&server.sin_addr.s_addr, he->h_length);
   server.sin_port = htons( atoi(argv[1]));
 
    //Connect to remote server
    if (connect(sock , (struct sockaddr *)&server , sizeof(server)) < ILLEGAL_CONNECT_RETURN_VALUE)
    {
		fprintf(stderr, "Error: function:connect errno:%d\n", errno);
		exit(1);
    }
    
    int fileDesc = open(argv[FILE_NAME], O_RDONLY);
	if (fileDesc == -1)
	{
		cout << CLIENT_USAGE_ERROR_MSG;
		exit(EXIT_FAILURE);
	}
    // discover file size
    struct stat st;
	stat(argv[FILE_NAME], &st);
    // if the given file is a dir
    bool isdir = S_ISDIR(st.st_mode);
	if (isdir)
	{
		cout << CLIENT_USAGE_ERROR_MSG;
		close(fileDesc);
		exit(EXIT_FAILURE);
	}
	long size = st.st_size;
	//first send - file size
	if (send(sock , &size , sizeof(long) , 0) <= MIN_LEGAL_TRANSFER_SIZE)
	{
		fprintf(stderr, "Error: function:send errno:%d\n", errno);
		close(fileDesc);
		exit(EXIT_FAILURE);
	}
	int valid, read_size;
	//recieve the validation if file size is fine or not
	read_size = recv(sock , &valid , sizeof(int), 0 );
	if (read_size == NO_DATA_TRANSFER)
	{
		fprintf(stderr, "Error: function:recv errno:%d\n", errno);
		close(fileDesc);
		exit(EXIT_FAILURE);
	}
	//case of file size is invalid
	if (!valid)
	{
		close(fileDesc);
		exit(EXIT_FAILURE);
	}
	
	int fileNameSize = strlen(argv[NEW_FILE_NAME]);
	
	// second send - file name size
	if (send(sock , &fileNameSize , sizeof(int) , 0) <= MIN_LEGAL_TRANSFER_SIZE)
	{
		fprintf(stderr, "Error: function:send errno:%d\n", errno);
		close(fileDesc);
		exit(EXIT_FAILURE);
	}
	
	//third send - new file name
	if (send(sock , argv[NEW_FILE_NAME] , fileNameSize + 1 , 0) <= MIN_LEGAL_TRANSFER_SIZE)
	{
		fprintf(stderr, "Error: function:send errno:%d\n", errno);
		close(fileDesc);
		exit(EXIT_FAILURE);
	}
	off_t offset = 0;
	int sent_bytes;
	int remain_data = st.st_size;
	/* Sending file data */
	while (((sent_bytes = sendfile(sock, fileDesc, &offset, BUFSIZ)) > MIN_LEGAL_TRANSFER_SIZE) && \
	(remain_data > MIN_LEGAL_TRANSFER_SIZE))
	{
			remain_data -= sent_bytes;
	}
	
	close(fileDesc);
	return EXIT_SUCCESS;
}
