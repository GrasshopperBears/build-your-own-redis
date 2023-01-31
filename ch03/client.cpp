#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

using namespace std;

int main(int argc, char* argv[])
{
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    cout << "Error on socket call";
    exit(1);
  }

  sockaddr_in addr = {};
  addr.sin_family = AF_INET;
  addr.sin_port = ntohs(8080);
  addr.sin_addr.s_addr = ntohl(INADDR_LOOPBACK);

  if (connect(fd, (const struct sockaddr*)&addr, sizeof(addr))) {
    cout << "Error on connect call";
    exit(1);
  }

  char msg[] = "hello server";
  write(fd, msg, strlen(msg));

  char read_buf[64] = {};
  if (read(fd, read_buf, sizeof(read_buf) - 1) < 0) {
    cout << "Error on read call";
    exit(1);
  }

  printf("Data from server: %s\n", read_buf);
  close(fd);
}
