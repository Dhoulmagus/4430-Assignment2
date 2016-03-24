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
#include <time.h>
#include "color.h"
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
  char IMS[100];
  int noCache;
};

char* getRequestLine(char* requestMessage)
{
  char requestMessageCopy[8192];
  strcpy(requestMessageCopy, requestMessage);
  printf("===requestMessageCopy===\n");
  printf("%s%s%s", BG_BLUE, requestMessageCopy, DEFAULT);
  printf("===requestMessageCopy end===\n");
  char* token;
  char* saveptr = malloc(200);
  token = strtok_r(requestMessageCopy, "\r\n", &saveptr);
  return token;
}

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

void getTimeStringfromRawTime(char* timeString, time_t rawtime)
{
  struct tm* testtmp = gmtime(&rawtime);
  strftime(timeString, 256, "%a, %d %b %Y %X %Z", testtmp);
}

void printMsg(char* msg)
{
  printf("%sclientRequest is: %s\n", BG_YELLOW, DEFAULT);
  printf("%s", BG_BLUE);
  printf("%s", msg);
  printf("%s", DEFAULT);
  printf("%sEnd of clientRequest%s\n", BG_YELLOW, DEFAULT);
}

time_t getRawTimefromTimeString(char* buf)
{
  struct tm testtm;
  strptime(buf, "%a, %d %b %Y %X %Z", &testtm);
  return mktime(&testtm)+8*60*60;
}

void hashURL(char* filename, char* URL)
{
  strncpy(filename, crypt(URL, "$1$00$")+6, 23);
  int i;
  for (i = 0; i < 22; ++i)
    if (filename[i] == '/')
      filename[i] = '_';
}

struct responseAttributes
{
  int statusCode;
  int contentLength;
  int isChunked;
  int serverClose;
  int isGzip;
};

struct responseAttributes parseResponseHeader(char* responseHeader)
{
  struct responseAttributes headerAttributes;

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

// int fileIsGzip(char* filepath)
// {
//   FILE* fp = fopen(filepath, "rb");
//   char oneByte;
//   int readSize = fread(&oneByte, 1, 1, fp);
//   if (readSize <= 0)
//     return -1;
//   if (oneByte != 0x1f)
//     return -1;
//
//   readSize = fread(&oneByte, 1, 1, fp);
//   if (readSize <= 0)
//     return -1;
// }



int main(int argc, char** argv)
{
  char buf[8192];
  // sprintf(buf, "GET http://www.cse.cuhk.edu.hk:32/diulei/fuck.pdf HTTP/1.1\r\n"
  // "Host: www.cse.cuhk.edu.hk\r\n"
  // "User-Agent: Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:45.0) Gecko/20100101 Firefox/45.0\r\n"
  // "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n"
  // "Accept-Language: en-US,en;q=0.8,zh-CN;q=0.7,zh-HK;q=0.5,ja;q=0.3,zh-TW;q=0.2\r\n"
  // "Accept-Encoding: gzip, deflate\r\n"
  // "Connection: close\r\n"
  // "\r\n");
  sprintf(buf, "HTTP/1.1 200 OK\r\n"
  "Date: Sun, 27 Feb 2011 04:09:33 GMT\r\n"
  "Transfer-Encoding: chunked\r\n"
  "Proxy-Connection: close\r\n\r\n");

  // printMsg(buf);
  //
  // struct requestAttributes a;
  // printf("notGet is: %d, typeNeedsCaching is: %d\n", a.notGET, a.typeNeedsCaching);

  //printf("%srequestLine is: %s%s\n", BG_YELLOW, getRequestLine(buf), DEFAULT);

  // parseRequestMessage(buf);
  //
  // struct hostent* hp = gethostbyname("www.cse.cuhk.edu.hk");
  // struct sockaddr_in addr;
  // memset(&addr, 0, sizeof(addr));
  // addr.sin_addr.s_addr = inet_addr(inet_ntoa(hp->h_addr));
  //
  // printf("gethostbyname: /%s/\n", inet_ntoa(hp->h_addr));

  // char anotherBuf[] = "Mon, 21 Oct 2002 12:30:20 GMT";
  // time_t IMS = getRawTimefromTimeString(anotherBuf);
  //
  // printf("IMS is now: /%ld/\n", IMS);
  //
  // char URLbuf[] = "http://www.cse.cuhk.edu.hk/";
  // char filename[23];
  // memset(filename, 0, sizeof(filename));
  // hashURL(filename, URLbuf);
  //
  // printf("filename is now: /%s/\n", filename);
  //
  // struct stat statbuf;
  // int statReturnValue = stat(cachePath, &statbuf);
  // if (statReturnValue == -1)
  // {
  //   printf("stat faliure!\n");
  // }
  // else
  // {
  //   printf("modification time is: /%ld/\n", statbuf.st_mtime);
  // }
  //
  // char timeString[256];
  // getTimeStringfromRawTime(timeString, IMS);
  // printf("time String from IMS is now: /%s/\n", timeString);

  struct responseAttributes b;
  b = parseResponseHeader(buf);

  return 0;
}
