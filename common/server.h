#include "common.h"

enum {
    STATE_REQ = 0,  // reading request
    STATE_RES = 1,  // sending response
    STATE_END = 2,
};

struct Conn {
    int fd = -1;
    uint32_t state = 0;

    size_t read_buf_size = 0;
    uint8_t read_buf[PROTOCOL_REQ_LEN + MAX_MSG];
    
    size_t write_buf_size = 0;
    size_t write_buf_sent = 0;
    uint8_t write_buf[PROTOCOL_REQ_LEN + MAX_MSG];
};

int initialize_server() {
    // Obtain socker fd
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        cout << "Error on socket call";
        exit(1);
    }

    // Config socket
    int val = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = ntohs(SERVICE_PORT);
    addr.sin_addr.s_addr = ntohl(0);

    if (bind(fd, (const sockaddr*)&addr, sizeof(addr)) > 0) {
        cout << "Error on bind call";
        exit(1);
    }

    if (listen(fd, SOMAXCONN)) {
        cout << "Error on listen call";
        exit(1);
    }
    return fd;
}

static void state_req(Conn* conn);
static void state_res(Conn* conn);
