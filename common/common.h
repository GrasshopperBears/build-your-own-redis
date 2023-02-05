#include <iostream>
#include <cstring>
#include<vector>
#include <map>
#include<cassert>

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h> 
#include <poll.h> 

#define SERVICE_PORT 8080
#define MAX_MSG 4096
#define MAX_ARGS 1024
#define PROTOCOL_REQ_LEN 4
#define PROTOCOL_RES_CODE_LEN 4

using namespace std;

enum {
    RES_OK = 0,
    RES_ERR = 1,
    RES_NX = 2,
};
