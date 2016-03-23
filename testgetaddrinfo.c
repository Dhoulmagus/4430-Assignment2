#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <pthread.h>
#include <netdb.h>
#include <sys/stat.h>
#include <crypt.h>
#include "color.h"

int connectToServer(char* Hostname, int port)
{
  struct hostent* hp = gethostbyname(Hostname);

  // if the host cannot be resolved, exit
  if (hp == NULL)
    return -1;

  struct in_addr *address = (struct in_addr*)(hp->h_addr);
  struct sockaddr_in serverAddr;
  memset(&serverAddr, 0, sizeof(serverAddr));
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_port = htons(port);
  serverAddr.sin_addr.s_addr = inet_addr(inet_ntoa(*address));

  //debug
  printf("connectToServer: ip is now: /%s/\n", inet_ntoa(*address));

  // Create socket
  int server_sd = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
  long serverVal = 1;
  if (setsockopt(server_sd, SOL_SOCKET, SO_REUSEADDR, &serverVal, sizeof(long)) == -1)
  {
    printf("%sconnectToServer: setsockopt error%s\n", BG_RED, DEFAULT);
    return -1;
  }

  // Connect to server
  if (connect(server_sd, (struct sockaddr*)&serverAddr, sizeof(struct sockaddr_in)) < 0)
  {
    printf("%sconnectToServer(): connect error%s\n", BG_RED, DEFAULT);
    return -1;
  }

  //debug
  printf("%sconnectToServer(): Established connection to %s(%s) Port %d%s\n", BG_GREEN, Hostname, inet_ntoa(*address), port, DEFAULT);
  printf("%sserver_sd is %d%s\n", BG_GREEN, server_sd, DEFAULT);

  return server_sd;

}


int main()
{
  printf("Input an address: \n");
  char address[100];
  scanf("%s", address);
  printf("The address is: /%s/\n", address);

  // struct addrinfo hints, *res;
  // int returnValue;
  // memset(&hints, 0, sizeof(hints));
  // hints.ai_family = AF_INET;
  // hints.ai_socktype = SOCK_STREAM;
  //
  // if ((returnValue = getaddrinfo(address, NULL, &hints, &res))!= 0)
  // {
  //   printf("%serror%s\n", BG_RED, DEFAULT);
  //   exit(-1);
  // }
  //
  // struct in_addr addr;
  // addr.s_addr = ((struct sockaddr_in*)res->ai_addr)->sin_addr.s_addr;

  //printf("ip is: /%s/\n", inet_ntoa(addr));


  connectToServer(address, 80);

  return 0;
}
