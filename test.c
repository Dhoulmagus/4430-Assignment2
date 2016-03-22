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
  char buf[8192];
  sprintf(buf, "GET http://www.cse.cuhk.edu.hk/ HTTP/1.1\r\n"
  "Host: www.cse.cuhk.edu.hk\r\n"
  "User-Agent: Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:45.0) Gecko/20100101 Firefox/45.0\r\n"
  "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n"
  "Accept-Language: en-US,en;q=0.8,zh-CN;q=0.7,zh-HK;q=0.5,ja;q=0.3,zh-TW;q=0.2\r\n"
  "Accept-Encoding: gzip, deflate\r\n"
  "Connection: keep-alive\r\n"
  "\r\n");


  return 0;
}
