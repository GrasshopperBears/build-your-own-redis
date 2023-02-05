#include "../common/client.h"

static int32_t send_req(int fd, const char* text) {
    uint32_t len = (uint32_t)strlen(text);
    if (len > MAX_MSG) {
        return -1;
    }

    char write_buf[PROTO_PAYLOAD_LEN + MAX_MSG];
    memcpy(write_buf, &len, PROTO_PAYLOAD_LEN);
    memcpy(&write_buf[PROTO_PAYLOAD_LEN], text, len);

    if (int32_t err = write_all(fd, write_buf, PROTO_PAYLOAD_LEN + len)) {
        return err;
    }
    return 0;
}

static int32_t read_res(int fd) {
    char read_buf[PROTO_PAYLOAD_LEN + MAX_MSG + 1];
    errno = 0;

    int32_t err = read_full(fd, read_buf, PROTO_PAYLOAD_LEN);
    if (err) {
        if (errno == 0) {
            cout << "EOF\n";
        } else {
            cout << "read() error\n";
        }
        return err;
    }

    uint32_t len = 0;
    memcpy(&len, read_buf, PROTO_PAYLOAD_LEN);
    if (len > MAX_MSG) {
        cout << "too long\n";
        return -1;
    }

    err = read_full(fd, &read_buf[PROTO_PAYLOAD_LEN], len);
    if (err) {
        cout << "read() error\n";
        return err;
    }

    read_buf[PROTO_PAYLOAD_LEN + len] = '\0';
    printf("Server response: %s\n", &read_buf[PROTO_PAYLOAD_LEN]);
    return 0;
}

int main() {
    int fd = initialize_client();

    // multiple requests w/pipelining
    const char *query_list[3] = {"hello1", "hello2", "hello3"};

    for (size_t i = 0; i < 3; ++i) {
        int32_t err = send_req(fd, query_list[i]);
        if (err) {
            cout << "Error on request\n";
            goto L_DONE;
        }
    }

    for (size_t i = 0; i < 3; ++i) {
        int32_t err = read_res(fd);
        if (err) {
            cout << "Error on reading response\n";
            goto L_DONE;
        }
    }

L_DONE:
    close(fd);
    return 0;
}
