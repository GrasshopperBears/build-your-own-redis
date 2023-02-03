#include <iostream>
#include <cstring>
#include<vector>
#include<cassert>

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h> 
#include <poll.h> 

#define MAX_MSG 4096
#define PROTOCOL_REQ_LEN 4

using namespace std;
