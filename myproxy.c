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

int main(int argc, char** argv)
{
  // Check if argument count is 2
  if (argc != 2)
  {
    printf("%sUsage: ./myproxy <Port>%s\n", BG_RED, DEFAULT);
    exit(-1);
  }
  int PORT = atoi(argv[1]);

  //debug
  printf("%smain(): PORT is now: %d%s\n", BG_YELLOW, PORT, DEFAULT);

  // Create socket
  int sd = socket(AF_INET, SOCK_STREAM, 0);
  // Make port reusable
  long val = 1;
  if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(long)) == -1)
  {
    perror("setsockopt");
    exit(-1);
  }

  int client_sd;
  struct sockaddr_in server_addr;
  struct sockaddr_in client_addr;

  memset(&server_addr,0,sizeof(server_addr));
  server_addr.sin_family=AF_INET;
  server_addr.sin_addr.s_addr=htonl(INADDR_ANY);
  server_addr.sin_port=htons(PORT);

  // Bind socket
  if(bind(sd,(struct sockaddr *) &server_addr,sizeof(server_addr))<0)
  {
    printf("bind error: %s (Errno:%d)\n",strerror(errno),errno);
    exit(0);
  }

  // Listen socket
  if(listen(sd,3)<0)
  {
    printf("listen error: %s (Errno:%d)\n",strerror(errno),errno);
    exit(0);
  }

  // Accept connection
  int addr_len = sizeof(client_addr);

  //debug
  printf("%sWaiting for Incoming connections...%s\n", BG_PURPLE, DEFAULT);

  if ((client_sd=accept(sd, (struct sockaddr *)&client_addr, &addr_len))<0)
  {
    printf("%sAccept error%s\n", BG_RED, DEFAULT);
    exit(-1);
  }

  //debug
  printf("%sAccepted client_sd: %d, sd: %d%s\n", BG_GREEN, client_sd, sd, DEFAULT);

  while(1)
  {
    char buf[1000];
    char payload[1000];
    memset(payload, 0, sizeof(payload));
    strcpy(buf, "");
    int bytesReceived;
    if((bytesReceived=read(client_sd,buf,sizeof(buf)))<0)
		{
			printf("receive error: %s (Errno:%d)\n", strerror(errno),errno);
			exit(-1);
		}

    printf("%sbuf is: %s\n", BG_YELLOW, DEFAULT);
    printf("%s", buf);
    printf("%sEnd of buf%s\n", BG_YELLOW, DEFAULT);
  }





  return 0;
}
