#include "../buildRedisCommon.h"

static void server_application(int connfd)
{
  char read_buf[64] = {};
  char write_buf[] = "Hi, I'm server";

  if (read(connfd, read_buf, sizeof(read_buf) - 1) < 0) {
    cout << "Error on read call";
  }
  printf("Data from client: %s\n", read_buf);
  write(connfd, write_buf, strlen(write_buf));
}

int main(int argc, char* argv[])
{
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
