#include "common.h"

int initialize_client() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        cout << "socket() error\n";
        exit(1);
    }

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = ntohs(8080);
    addr.sin_addr.s_addr = ntohl(INADDR_LOOPBACK);

    if (connect(fd, (const struct sockaddr*)&addr, sizeof(addr))) {
        cout << "Error on connect call\n";
        exit(1);
    }
    return fd;
}
