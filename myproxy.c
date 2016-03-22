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

struct requestAttributes
{
  int methodNotGET;
  int typeNeedsCaching;
  int port;
  char extension[10];
  char Host[100];
  char URL[200];
  int clientClose;
  char IMS[100];
  int noCache;
};

struct requestAttributes parseRequestMessage(char* requestMessage)
{
  struct requestAttributes messageAttributes;

  // Get ready to strtok
  char requestMessageCopy[8192];
  strcpy(requestMessageCopy, requestMessage);
  char* lineSavePtr = malloc(200);
  char* wordSavePtr = malloc(100);
  char* tofree1 = lineSavePtr;
  char* tofree2 = wordSavePtr;
  char* lineToken;
  char* wordToken;

  // Parse and get the first Line, i.e. the Request Line
  lineToken = strtok_r(requestMessageCopy, "\r\n", &lineSavePtr);
  char requestLine[200];
  strcpy(requestLine, lineToken);
  //debug
  printf("Request Line is: %s%s%s\n", BG_YELLOW, requestLine, DEFAULT);

  // Parse the first line to get method
  char requestLineCopy[200];
  strcpy(requestLineCopy, requestLine);
  wordToken = strtok_r(requestLineCopy, " ", &wordSavePtr);
  char method[15];
  strcpy(method, wordToken);
  //debug
  printf("Method is: %s%s%s\n", BG_YELLOW, method, DEFAULT);
  if (strcmp(method, "GET") != 0)
    messageAttributes.methodNotGET = 1;

  // Parse the first line again to get URL
  wordToken = strtok_r(NULL, " ", &wordSavePtr);
  char URL[200];
  strcpy(URL, wordToken);
  strcpy(messageAttributes.URL, URL);
  //debug
  printf("URL is: %s%s%s\n", BG_YELLOW, URL, DEFAULT);

  // Parse the URL to get the port
  char URLCopy[200];
  strcpy(URLCopy, URL);
  char* pointerToColon = strchr(URLCopy+5, ':'); // Move the pointer 5 byte behind to avoid checking the colon in http:
  if (pointerToColon == NULL) // ':' is not found, so port is default 80
  {
    messageAttributes.port = 80;
    wordToken = strtok_r(URLCopy+7, ":/", &wordSavePtr);
  }
  else // ':' is found, and extract the port from the URL
  {
    wordToken = strtok_r(pointerToColon, ":/", &wordSavePtr);
    messageAttributes.port = atoi(wordToken);
  }
  //debug
  printf("port is: %s%d%s\n", BG_YELLOW, messageAttributes.port, DEFAULT);

  // Keep Parsing the URL to get the file name
  char fileName[100];
  while ((wordToken = strtok_r(NULL, ":/", &wordSavePtr)) != NULL)
  {
    memset(fileName, 0, sizeof(fileName));
    strcpy(fileName, wordToken);
  }
  printf("fileName is: %s%s%s\n", BG_YELLOW, fileName, DEFAULT);

  // Parse the file name to get the extension
  char extension[10];
  if (strcmp(fileName, "") == 0)
  {
    strcpy(extension, "html");
  }
  else
  {
    char fileNameCopy[100];
    strcpy(fileNameCopy, fileName);
    wordToken = strtok_r(fileNameCopy, ".", &wordSavePtr);
    while ((wordToken = strtok_r(NULL, ".", &wordSavePtr)) != NULL)
    {
      memset(extension, 0, sizeof(extension));
      strcpy(extension, wordToken);
    }
  }
  strcpy(messageAttributes.extension, extension);
  //debug
  printf("extension is: %s%s%s\n", BG_YELLOW, messageAttributes.extension, DEFAULT);
  if (strcmp(extension, "html") == 0 || strcmp(extension, "jpg") == 0 || strcmp(extension, "gif") == 0 ||
      strcmp(extension, "txt")  == 0 || strcmp(extension, "pdf") == 0)
      messageAttributes.typeNeedsCaching = 1;

  // Here processing the Request Line is done.
  // Now parse and get Header Lines
  while ((lineToken = strtok_r(NULL, "\r\n", &lineSavePtr)) != NULL)
  {
    char lineCopy[200];
    strcpy(lineCopy, lineToken);

    // Get Header Name
    char* headerName = strtok_r(lineCopy, ":", &wordSavePtr);


    // Host: ...
    if (strcmp(headerName, "Host") == 0)
    {
      wordToken = strtok_r(NULL, ":", &wordSavePtr);
      strcpy(messageAttributes.Host, wordToken+1);
      //debug
      printf("Host is: %s%s%s\n", BG_YELLOW, messageAttributes.Host, DEFAULT);
    }
    // Proxy-Connection = close
    // or Connection = close
    else if (strcmp(headerName, "Proxy-Connection") == 0 || strcmp(headerName, "Connection") == 0)
    {
      if (strstr(lineToken, "close") != NULL)
        messageAttributes.clientClose = 1;
    }
    // If-Modified-Since: ...
    else if (strcmp(headerName, "If-Modified-Since") == 0)
    {
      wordToken = strtok_r(NULL, ":", &wordSavePtr);
      strcpy(messageAttributes.IMS, wordToken+1);
      //debug
      printf("IMS is: %s%s%s\n", BG_YELLOW, messageAttributes.IMS, DEFAULT);
    }
    // Cache-Control: no-cache
    else if (strcmp(headerName, "Cache-Control") == 0)
    {
      if (strstr(lineToken, "no-cache") != NULL)
        messageAttributes.noCache = 1;
    }
  }

  free(tofree1);
  free(tofree2);
  return messageAttributes;
}




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
  unsigned int addr_len = sizeof(client_addr);
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
    printf("%s", DEFAULT);
    printf("%sEnd of clientRequest%s\n", BG_YELLOW, DEFAULT);

    struct requestAttributes clientRequestAttributes = parseRequestMessage(clientRequest);

    // If the method is not GET, simply ignore it
    if (clientRequestAttributes.methodNotGET)
      continue;
    // If the file type is not the ones that need cacheing,
    // simply forwards it to the web server
    
  }







  return 0;
}
