#include "../buildRedisCommon.h"

int main(int argc, char* argv[])
{
    int fd = initialize_client();

    char msg[] = "hello server";
    write(fd, msg, strlen(msg));

    char read_buf[MAX_MSG] = {};
    if (read(fd, read_buf, sizeof(read_buf) - 1) < 0) {
        cout << "Error on read call\n";
        exit(1);
    }

    printf("Data from server: %s\n", read_buf);
    close(fd);
}
