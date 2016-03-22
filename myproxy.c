#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <dirent.h>
#include <pthread.h>
#include <netdb.h>
#include <sys/stat.h>
#include <crypt.h>
#include "color.h"
#define MAX_REQUEST_SIZE 8192






int main(int argc, char** argv)
{
  // Check if argument count is 2
  if (argc != 2)
  {
    printf("%sUsage: ./myproxy <Port>%s\n", BG_RED, DEFAULT);
    exit(-1);
  }
  //TO-DO: Check if it is a valid port number
  int PORT = atoi(argv[1]);

  //debug
  printf("%smain(): PORT is now: %d%s\n", BG_YELLOW, PORT, DEFAULT);

  // Create listening socket
  int sd = socket(AF_INET, SOCK_STREAM, 0);
  // Make port reusable
  long val = 1;
  if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(long)) == -1)
  {
    perror("setsockopt");
    exit(-1);
  }

  struct sockaddr_in server_addr;

  memset(&server_addr,0,sizeof(server_addr));
  server_addr.sin_family=AF_INET;
  server_addr.sin_addr.s_addr=htonl(INADDR_ANY);
  server_addr.sin_port=htons(PORT);

  // Bind socket
  if(bind(sd,(struct sockaddr *) &server_addr,sizeof(server_addr))<0)
  {
    printf("bind error: %s (Errno:%d)\n",strerror(errno),errno);
    exit(-1);
  }

  // Listen socket
  if(listen(sd,3)<0)
  {
    printf("listen error: %s (Errno:%d)\n",strerror(errno),errno);
    exit(-1);
  }

  //debug
  printf("%sWaiting for Incoming connections...%s\n", BG_PURPLE, DEFAULT);

  // Accept connection
  struct sockaddr_in client_addr;
  int addr_len = sizeof(client_addr);
  int client_sd;
  if ((client_sd=accept(sd, (struct sockaddr *)&client_addr, &addr_len))<0)
  {
    printf("%sAccept error%s\n", BG_RED, DEFAULT);
    exit(-1);
  }

  //debug
  printf("%sAccepted client_sd: %d, sd: %d%s\n", BG_GREEN, client_sd, sd, DEFAULT);

  while(1)
  {
    char clientRequest[MAX_REQUEST_SIZE];
    //char payload[1000];
    //memset(payload, 0, sizeof(payload));
    strcpy(clientRequest, "");
    int bytesReceived = read(client_sd, clientRequest, sizeof(clientRequest));
    if(bytesReceived < 0)
		{
			printf("%sreceive error: %s (Errno:%d)%s\n",BG_RED, strerror(errno), errno, DEFAULT);
			exit(-1);
		}
    else if (bytesReceived == 0)
    {
      printf("%sClient closed the connection. %s\n", BG_YELLOW, DEFAULT);
      exit(0);
    }

    printf("%sclientRequest is: %s\n", BG_YELLOW, DEFAULT);
    printf("%s", BG_BLUE);
    printf("%s", clientRequest);
    printf("%s\n", DEFAULT);
    printf("%sEnd of clientRequest%s\n", BG_YELLOW, DEFAULT);
  }







  return 0;
}
