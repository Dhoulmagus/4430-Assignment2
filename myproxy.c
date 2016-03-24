#define _XOPEN_SOURCE
#define _GNU_SOURCE
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
#include <arpa/inet.h>
#include <time.h>
#include "color.h"
#define MAX_REQUEST_SIZE 8192
#define MAX_RESPONSE_SIZE 8192
#define cachePath "./cache/"

struct requestAttributes
{
  int methodNotGET;
  int typeNeedsCaching;
  int port;
  char extension[10];
  char Host[100];
  char URL[200];
  int clientClose;
  time_t IMS;
  int noCache;
};

void printRequestAttributes(struct requestAttributes* a)
{
  printf("=== request Attributes ===\n");
  printf("methodNotGET is %s%d%s\n", BG_BLUE, a->methodNotGET, DEFAULT);
  printf("typeNeedsCaching is %s%d%s\n", BG_BLUE, a->typeNeedsCaching, DEFAULT);
  printf("port is %s%d%s\n", BG_BLUE, a->port, DEFAULT);
  printf("extension is %s%s%s\n", BG_BLUE, a->extension, DEFAULT);
  printf("Host is %s%s%s\n", BG_BLUE, a->Host, DEFAULT);
  printf("URL is %s%s%s\n", BG_BLUE, a->URL, DEFAULT);
  printf("clientClose is %s%d%s\n", BG_BLUE, a->clientClose, DEFAULT);
  printf("IMS is %s%ld%s\n", BG_BLUE, a->IMS, DEFAULT);
  printf("noCache is %s%d%s\n", BG_BLUE, a->noCache, DEFAULT);
  printf("=== End of request Attributes ===\n");
}


/**Input a time string, say, "Mon, 21 Oct 2002 12:30:20 GMT"
 * Then this function will return a raw long time
 */
time_t getRawTimefromTimeString(char* buf)
{
  struct tm testtm;
  strptime(buf, "%a, %d %b %Y %X %Z", &testtm);
  return mktime(&testtm)+8*60*60;
}

/**Input a raw long time and store the formatted time string into timeString[50]
 * that can be used as IMS header
 */
void getTimeStringfromRawTime(char* timeString, time_t rawtime)
{
  struct tm* testtmp = gmtime(&rawtime);
  strftime(timeString, 50, "%a, %d %b %Y %X %Z", testtmp);
}


/**Extract useful info from the client's HTTP Request
 * And store into struct requestAttributes
 */
struct requestAttributes parseRequestMessage(char* requestMessage)
{
  struct requestAttributes messageAttributes
  = {0, 0, 0, "", "", "", 0, 0, 0};

  //printRequestAttributes(&messageAttributes);

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
      messageAttributes.IMS = getRawTimefromTimeString(wordToken+1);
      //debug
      printf("IMS is: %s%ld%s\n", BG_YELLOW, messageAttributes.IMS, DEFAULT);
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
  // printRequestAttributes(&messageAttributes);
  return messageAttributes;
}

/**Establish a connection to server
 * Return the socket descriptor of the connection between MYPROXY and server
 */
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
  int server_sd = socket(AF_INET,SOCK_STREAM,0);
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

/**hash URL into filename[23]
 */
void hashURL(char* filename, char* URL)
{
  strncpy(filename, crypt(URL, "$1$00$")+6, 23);
  int i;
  for (i = 0; i < 22; ++i)
    if (filename[i] == '/')
      filename[i] = '_';
}

/**If the file does not exist, return -1
 * Otherwise, return the raw long time of the Last Modified Time of this file
 */
time_t getFileModificationTime(char* URL)
{
  // First, hash the URL to filename
  char filename[23];
  memset(filename, 0, sizeof(filename));
  hashURL(filename, URL);

  // Then check if the file, whose name is filename, exists
  struct stat statbuf;
  int statReturnValue = stat(cachePath, &statbuf);
  if (statReturnValue == -1)
  {
    mkdir(cachePath, 0777);
    return -1;
  }

  char filepath[50];
  memset(filepath, 0, sizeof(filepath));
  strcpy(filepath, cachePath);
  strcat(filepath, filename);

  //debug
  printf("getFileModificationTime(): filepath is |%s%s%s|\n", BG_PURPLE, filepath, DEFAULT);

  memset(&statbuf, 0, sizeof(statbuf));
  statReturnValue = stat(filepath, &statbuf);
  if (statReturnValue == -1) // The file does not exist
    return -1;
  return statbuf.st_mtime;
}

/**respond the cache to client
 * which contains BOTH the HTTP Response Header
 * AND the Response Body
 * i.e. the whole HTTP Response from the server
 */
int respondCache(char* URL, int client_sd)
{
  char filename[23];
  memset(filename, 0, sizeof(filename));
  hashURL(filename, URL);
  char filepath[50];
  memset(filepath, 0, sizeof(filepath));
  strcpy(filepath, cachePath);
  strcat(filepath, filename);

  char block[512];

  FILE* fp = fopen(filepath, "rb");
  int readSize = 0;
  do {
    memset(block, 0, sizeof(block));
    readSize = fread(block, 1, 512, fp);
    if (readSize > 0)
      if (send(client_sd, block, readSize, MSG_NOSIGNAL) <= 0)
        return -1;
  } while(readSize > 0);
  fclose(fp);
  return 0;
}

int respondTempFile(int client_sd)
{
  char block[512];
  FILE* fp = fopen("./cache/temp", "rb");
  int readSize = 0;
  do {
    memset(block, 0, sizeof(block));
    readSize = fread(block, 1, sizeof(block), fp);
    if (readSize > 0)
      if (send(client_sd, block, readSize, MSG_NOSIGNAL) <= 0)
        return -1;
  } while (readSize > 0);
  fclose(fp);
  return 0;
}

struct responseAttributes
{
  int statusCode;
  int contentLength;
  int isChunked;
  int serverClose;
  int isGzip;
};

void printResponseAttributes(struct responseAttributes* a)
{
  printf("=== response Attributes ===\n");
  printf("statusCode is %s%d%s\n", BG_BLUE, a->statusCode, DEFAULT);
  printf("contentLength is %s%d%s\n", BG_BLUE, a->contentLength, DEFAULT);
  printf("isChunked is %s%d%s\n", BG_BLUE, a->isChunked, DEFAULT);
  printf("serverClose is %s%d%s\n", BG_BLUE, a->serverClose, DEFAULT);
  printf("isGzip is %s%d%s\n", BG_BLUE, a->isGzip, DEFAULT);
  printf("=== End of response Attributes ===\n");
}

struct responseAttributes parseResponseHeader(char* responseHeader)
{
  struct responseAttributes headerAttributes
  = {0, 0, 0, 0, 0};

  char responseHeaderCopy[8192];
  strcpy(responseHeaderCopy, responseHeader);
  char* lineSavePtr = malloc(200);
  char* wordSavePtr = malloc(100);
  char* tofree1 = lineSavePtr;
  char* tofree2 = wordSavePtr;
  char* lineToken;
  char* wordToken;

  // Parse and get the first line, i.e. the Status Line
  lineToken = strtok_r(responseHeaderCopy, "\r\n", &lineSavePtr);
  char statusLine[100];
  strcpy(statusLine, lineToken);
  //debug
  printf("parseResponseHeader(): Status Line is: /%s%s%s/\n", BG_YELLOW, statusLine, DEFAULT);

  // Parse the first lint to get Status Code
  char statusLineCopy[100];
  strcpy(statusLineCopy, statusLine);
  wordToken = strtok_r(statusLineCopy, " ", &wordSavePtr);
  wordToken = strtok_r(NULL, " ", &wordSavePtr);
  headerAttributes.statusCode = atoi(wordToken);

  //debug
  printf("parseResponseHeader(): Status code is: /%s%d%s/\n", BG_YELLOW, headerAttributes.statusCode, DEFAULT);

  // Done processing the Status Line here
  // Now parse and get Header Lines
  while ((lineToken = strtok_r(NULL, "\r\n", &lineSavePtr)) != NULL)
  {
    char lineCopy[200];
    strcpy(lineCopy, lineToken);

    // Get Header Name
    char* headerName = strtok_r(lineCopy, ":", &wordSavePtr);

    // Content-Length: ...
    if (strcmp(headerName, "Content-Length") == 0)
    {
      wordToken = strtok_r(NULL, ":", &wordSavePtr);
      headerAttributes.contentLength = atoi(wordToken);
      //debug
      printf("contentLength is: /%s%d%s/\n", BG_YELLOW, headerAttributes.contentLength, DEFAULT);

    }

    // Transfer-Encoding: chunked
    else if (strcmp(headerName, "Transfer-Encoding") == 0)
    {
      if (strstr(lineToken, "chunked") != NULL)
      {
        headerAttributes.isChunked = 1;
        printf("Transfer-Encoding is chunked\n");
      }
    }

    // Proxy-Connection = close
    // or Connection = close
    else if (strcmp(headerName, "Proxy-Connection") == 0 || strcmp(headerName, "Connection") == 0)
    {
      if (strstr(lineToken, "close") != NULL)
      {
        headerAttributes.serverClose = 1;
        printf("Server respond connection close.\n");
      }
    }

    // Content-Encoding: gzip
    else if (strcmp(headerName, "Content-Encoding") == 0)
    {
      if (strstr(lineToken, "gzip") != NULL)
      {
        headerAttributes.isGzip = 1;
        printf("Content is gzip");
      }
    }
  }

  free(tofree1);
  free(tofree2);
  return headerAttributes;
}

/** Store the whole server response to both cache and a temp file
 */
int cacheServerResponse(char* responseHeader, char* URL, int server_sd, int has_no_body)
{
  char responseHeaderCopy[MAX_RESPONSE_SIZE];
  strcpy(responseHeaderCopy, responseHeader);

  char filename[23];
  memset(filename, 0, sizeof(filename));
  hashURL(filename, URL);
  char filepath[50];
  memset(filepath, 0, sizeof(filepath));
  strcpy(filepath, cachePath);
  strcat(filepath, filename);

  // First write the header part of the response to both cache file and temp file
  FILE* fp = fopen(filepath, "wb");
  FILE* temp = fopen("./cache/temp", "wb");
  int byteWritten = fwrite(responseHeaderCopy, 1, strlen(responseHeaderCopy), fp);
  byteWritten = fwrite(responseHeaderCopy, 1, strlen(responseHeaderCopy), temp);
  if (byteWritten == 0)
    return -1; // fwrite error?

  // Then if the response has body, continue to write the body part to file
  if (!has_no_body)
  {
    char block[1024];
    int totalSize = 0;
    // Read body from server block by block
    while(1)
    {
      memset(block, 0, sizeof(block));
      int readSize = read(server_sd, block, sizeof(block));
      printf("readSize is now: /%s%d%s/\n", BG_YELLOW, readSize, DEFAULT);
      if (readSize < 0) // read error?
        return -1;
      if (readSize == 0) // has read up the response body
        break;

      //debug
      printf("block is now: /%s%s%s/\n", BG_BLUE, block, DEFAULT);

      // Write the body block to both cache and temp
      fwrite(block, 1, readSize, fp);
      fwrite(block, 1, readSize, temp);

      totalSize += readSize;
    }

    printf("%scacheServerResponse(): received %d bytes of body in total%s\n", BG_PURPLE, totalSize, DEFAULT);
  }

  fclose(temp);
  fclose(fp);
  return 0;
}

/** Store the whole server response to only a temp file
 */
int temporarilyStoreServerResponse(char* responseHeader, int server_sd, int has_no_body)
{
  char responseHeaderCopy[MAX_RESPONSE_SIZE];
  strcpy(responseHeaderCopy, responseHeader);

  FILE* temp = fopen("./cache/temp", "wb");
  int byteWritten = fwrite(responseHeaderCopy, 1, strlen(responseHeaderCopy), temp);
  if (byteWritten == 0)
    return -1;


  if (!has_no_body)
  {
    char block[1024];
    int totalSize = 0;
    // Read body from server block by block
    while(1)
    {
      memset(block, 0, sizeof(block));
      int readSize = read(server_sd, block, sizeof(block));
      if (readSize < 0) // read error?
        return -1;
      if (readSize == 0) // has read up the response body
        break;

      // Write the body block to temp
      fwrite(block, 1, readSize, temp);

      totalSize += readSize;
    }

    printf("%stemporarilyStoreServerResponse(): received %d bytes of body in total%s\n", BG_PURPLE, totalSize, DEFAULT);
  }
  fclose(temp);
  return 0;
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
  //printf("%smain(): PORT is now: %d%s\n", BG_YELLOW, PORT, DEFAULT);

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
  printf("%sAccepted client_sd: %d, IP: %s%s\n", BG_GREEN, client_sd, inet_ntoa(client_addr.sin_addr), DEFAULT);

  // Flags for the loop
  int server_sd;
  int server_sd_reusable = 0;
  int server_response_has_no_body;
  int is_case_4;
  int file_already_cached;

  while(1)
  {
    // Initialize flags
    server_response_has_no_body = 0;
    is_case_4 = 0;
    file_already_cached = 0;

    // Read HTTP Request from the client
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
      printf("%sClient closed the connection. %s\n", BG_RED, DEFAULT);
      exit(0);
    }

    //debug
    printf("%s=== clientRequest ===%s\n", BG_YELLOW, DEFAULT);
    printf("/%s%s%s/\n",BG_BLUE, clientRequest, DEFAULT);
    printf("%s=== End of clientRequest ===%s\n", BG_YELLOW, DEFAULT);

    // Parse the HTTP Request to struct requestAttributes
    struct requestAttributes clientRequestAttributes = parseRequestMessage(clientRequest);

    // If server_sd can be reused, reuse it
    // Else, establish a connection to server
    if (server_sd_reusable)
      printf("Server sd is reusable. Reuse server_sd: /%s%d%s/ now. \n", BG_PURPLE, server_sd, DEFAULT);
    else
      server_sd = connectToServer(clientRequestAttributes.Host, clientRequestAttributes.port);
    server_sd_reusable = 1;

    char buf[MAX_REQUEST_SIZE];
    strcpy(buf, clientRequest);

    //debug
    printRequestAttributes(&clientRequestAttributes);

    // If the method is not GET, simply ignore it
    if (clientRequestAttributes.methodNotGET)
    {
      printf("%sIgnored a non-GET request.%s\n", BG_PURPLE, DEFAULT);
      continue;
    }

    // If the file type is not the ones that need caching,
    // simply forwards the request to the web server
    if (!clientRequestAttributes.typeNeedsCaching)
    {
      printf("%sThe request file type does not need caching. Simply forward the request to server now.%s\n", BG_PURPLE, DEFAULT);
      int byteSent = send(server_sd, buf, strlen(buf), MSG_NOSIGNAL);
      printf("Sent /%s%d%s/ bytes to the server. \n", BG_PURPLE, byteSent, DEFAULT);
      if (byteSent < 0)
      {
        printf("%sForward HTTP Request Error%s\n", BG_RED, DEFAULT);
        continue;
      }
    }
    else
    {
      // If the file is not cached in MYPROXY,
      // simply forwards it to the web server
      if (getFileModificationTime(clientRequestAttributes.URL) == -1)
      {
        printf("%sThe file is not cached on the proxy. Simply forward the request to server now. %s\n", BG_PURPLE, DEFAULT);
        int byteSent = send(server_sd, buf, strlen(buf), MSG_NOSIGNAL);
        if (byteSent < 0)
        {
          printf("%sForward HTTP Request Error%s\n", BG_RED, DEFAULT);
          continue;
        }
      }
      else
      {
        file_already_cached = 1;

        // Here goes the four cases
        int caseNumber;
        if (clientRequestAttributes.noCache)
        {
          if (clientRequestAttributes.IMS == 0)
            caseNumber = 3;
          else
            caseNumber = 4;
        }
        else
        {
          if (clientRequestAttributes.IMS == 0)
            caseNumber = 1;
          else
            caseNumber = 2;
        }

        //debug
        printf("%scaseNumber is: %d%s\n", BG_PURPLE, caseNumber, DEFAULT);

        // didn't use switch since I fear to mess up
        // the break in switch with the break in while
        if (caseNumber == 1)
        {
          // Case 1:
          // Proxy directly respond the client with the cache
          if (respondCache(clientRequestAttributes.URL, client_sd) == -1)
            break;

          goto oneLoopisDone;
        }

        if (caseNumber == 2)
        {
          // Case 2:
          // If IMS is later than LMT of the cached web object,
          // just respond 304
          // else, respond the client with 200 and the web object
          if (clientRequestAttributes.IMS > getFileModificationTime(clientRequestAttributes.URL))
          {
            char block[512];
            sprintf(block, "HTTP/1.1 304 Not Modified\r\n\r\n");
            if (send(client_sd, block, strlen(block), MSG_NOSIGNAL) <= 0)
              break;
          }
          else
            if (respondCache(clientRequestAttributes.URL, client_sd) == -1)
              break;

          goto oneLoopisDone;
        }

        if (caseNumber == 3)
        {
          // Case 3:
          // Add If-Modified-Since: *LMT* header Line to the client Request
          // and then forward it to server
          time_t LMT = getFileModificationTime(clientRequestAttributes.URL);
          char timeString[50];
          getTimeStringfromRawTime(timeString, LMT);
          char* headerEnding = strstr(buf, "\r\n\r\n");
          *headerEnding = '\0';
          sprintf(buf, "\r\nIf-Modified-Since: %s\r\n\r\n", timeString);

          //debug
          printf("%s=== case 3 buf ===%s\n", BG_YELLOW, DEFAULT);
          printf("/%s%s%s/\n", BG_BLUE, buf, DEFAULT);
          printf("%s=== End of case 3 buf ===%s\n", BG_YELLOW, DEFAULT);

          int byteSent = send(server_sd, buf, strlen(buf), MSG_NOSIGNAL);
          if (byteSent < 0)
          {
            printf("%sForward HTTP Request Error%s\n", BG_RED, DEFAULT);
            continue;
          }
        }

        if (caseNumber == 4)
        {
          // Case 4:
          // If IMS is later than LMT of the cached web object,
          // simply forwards it to the web server
          // Else, edit If-Modified-Since: ... header Line of the client request
          // edit the date to LMT
          // and then forward it to the web server
          is_case_4 = 1;

          time_t LMT = getFileModificationTime(clientRequestAttributes.URL);
          if (clientRequestAttributes.IMS < LMT)
          {
            char* pointerToIMS = strstr(buf, "If-Modified-Since");
            char* pointerToLatterPart = strstr(pointerToIMS, "\r\n");
            char latterPart[MAX_REQUEST_SIZE];
            strcpy(latterPart, pointerToLatterPart);
            pointerToIMS+=19; // Maybe problematic?
            *pointerToIMS = '\0';
            char timeString[50];
            getTimeStringfromRawTime(timeString, LMT);
            strcat(buf, timeString);
            strcat(buf, latterPart);

            //debug
            printf("%s=== case 4 buf ===%s\n", BG_YELLOW, DEFAULT);
            printf("/%s%s%s/\n", BG_BLUE, buf, DEFAULT);
            printf("%s=== End of case 4 buf ===%s\n", BG_YELLOW, DEFAULT);
          }

          int byteSent = send(server_sd, buf, strlen(buf), MSG_NOSIGNAL);
          if (byteSent < 0)
          {
            printf("%sForward HTTP Request Error%s\n", BG_RED, DEFAULT);
            continue;
          }
        }
      }
    }

    // Now receive server's HTTP Response's header
    char receiveBuffer[MAX_RESPONSE_SIZE];
    memset(receiveBuffer, 0, sizeof(receiveBuffer));
    char* receiveBufferPtr = receiveBuffer;

    printf("%sWaiting for the server's response...%s\n", BG_PURPLE, DEFAULT);

    // Receive byte by byte until an \r\n\r\n is found
    while(1)
    {
      bytesReceived = recv(server_sd, receiveBufferPtr, 1, 0);
      if (bytesReceived < 0)
      {
        printf("%sreceive HTTP response error%s\n", BG_RED, DEFAULT);
        break;
      }

      //debug
      if (bytesReceived == 0)
      {
        printf("fuck ");
        exit(-1);
      }

      printf("%sReceived one byte%s ", BG_PURPLE, DEFAULT);

      if (strstr(receiveBuffer, "\r\n\r\n") != NULL)
        break;
      receiveBufferPtr++;
    }
    receiveBufferPtr++;
    *receiveBufferPtr = '\0';

    //debug
    printf("\n%s=== server's response's header ===%s\n", BG_YELLOW, DEFAULT);
    printf("/%s%s%s/\n", BG_BLUE, receiveBuffer, DEFAULT);
    printf("%s=== End of server's response's header ===%s\n", BG_YELLOW, DEFAULT);

    // parse server's response's header
    struct responseAttributes serverResponseAttributes = parseResponseHeader(receiveBuffer);

    printResponseAttributes(&serverResponseAttributes);


    int response_has_no_body = 0;
    if (serverResponseAttributes.contentLength == 0 && serverResponseAttributes.isChunked == 0)
      response_has_no_body = 1;

    // 200 case
    if (serverResponseAttributes.statusCode == 200)
    {
      // If the file type needs cacheing, then cache it
      if (clientRequestAttributes.typeNeedsCaching)
      {
        if (cacheServerResponse(receiveBuffer, clientRequestAttributes.URL, server_sd, response_has_no_body) == -1)
        {
          printf("%scache Error%s\n", BG_RED, DEFAULT);
          break;
        }
      }
      // Else, simply store the response
      else
      {
        if (temporarilyStoreServerResponse(receiveBuffer, server_sd, response_has_no_body) == -1)
        {
          printf("%sstore Error%s\n", BG_RED, DEFAULT);
          break;
        }
      }

      printf("%sok here?2%s\n", FG_RED, DEFAULT);
    }
    // 304 case
    else if (serverResponseAttributes.statusCode == 304)
    {
      if (is_case_4)
      {
        if (respondCache(clientRequestAttributes.URL, client_sd) == -1)
        {
          printf("%srespond Error%s\n", BG_RED, DEFAULT);
          break;
        }


        goto forwardBackComplete;
      }
      else
      {
        if (temporarilyStoreServerResponse(receiveBuffer, server_sd, response_has_no_body) == -1)
        {
          printf("%sstore Error%s\n", BG_RED, DEFAULT);
          break;
        }
      }
    }
    // Status code is otherwise case
    else
    {
      if (temporarilyStoreServerResponse(receiveBuffer, server_sd, response_has_no_body) == -1)
      {
        printf("%sstore Error%s\n", BG_RED, DEFAULT);
        break;
      }
    }

    printf("ok here 3\n");

    // Now forward the response back to client
    if (respondTempFile(client_sd) == -1)
    {
      printf("%sclient closed the connection%s\n", BG_RED, DEFAULT);
      break;
    }

    forwardBackComplete:

    if (serverResponseAttributes.serverClose)
    {
      printf("%sThe server's response has Connection: close. Close server_sd now. %s\n", BG_PURPLE, DEFAULT);
      server_sd_reusable = 0;
      close(server_sd);
    }


    oneLoopisDone:
    if (server_response_has_no_body)
      break;

    if (clientRequestAttributes.clientClose)
      break;

    printf("%sOne Loop(c->p->s->p->c) is done.%s\n", BG_PURPLE, DEFAULT);

  }
  close(server_sd);
  close(client_sd);


  return 0;
}
