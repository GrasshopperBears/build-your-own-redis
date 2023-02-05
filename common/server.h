#include "common.h"

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
    addr.sin_port = ntohs(8080);
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