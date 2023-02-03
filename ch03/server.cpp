#include "../buildRedisCommon.h"

static void server_application(int connfd)
{
    char read_buf[64] = {};
    char write_buf[] = "Hi, I'm server\n";

    if (read(connfd, read_buf, sizeof(read_buf) - 1) < 0) {
        cout << "Error on read call\n";
    }
    printf("Data from client: %s\n", read_buf);
    write(connfd, write_buf, strlen(write_buf));
}

int main(int argc, char* argv[])
{
    int fd = initialize_server();

    while (true) {
        sockaddr_in client_addr = {};
        socklen_t socklen = sizeof(client_addr);

        int connfd = accept(fd, (sockaddr*)&client_addr, &socklen);
        if (connfd < 0) {
            continue;
        }

        server_application(connfd);
        close(connfd);
    }
}
