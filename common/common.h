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

inline const char* rescode2str(uint32_t rescode) {
    switch (rescode)
    {
        case 0: return "RES_OK";
        case 1: return "RES_ERR";
        case 2: return "RES_NX";
        default: return "Illegal res_status";
    }
}

static void println(string msg) {
    cout << msg << '\n';
}

static uint32_t println_and_return(string msg, uint32_t ret_val) {
    println(msg);
    return ret_val;
}
