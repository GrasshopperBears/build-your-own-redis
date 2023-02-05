#include "../buildRedisCommon.h"

static int32_t write_all(int fd, const char* buf, size_t n) {
    while (n > 0) {
        ssize_t rv = write(fd, buf, n);
        if (rv <= 0) {
            return -1;
        }
        assert((size_t)rv <= n);
        n -= (size_t)rv;
        buf += rv;
    }
    return 0;
}

static int32_t send_req(int fd, const vector<string> &cmd) {
    uint32_t len = 4;
    for (const string &s: cmd) {
        len += 4 + s.size();
    }

    if (len > MAX_ARGS) { return -1; }

    char write_buf[4 + MAX_ARGS];
    uint32_t n = cmd.size();

    memcpy(&write_buf[0], &len, 4);
    memcpy(&write_buf[4], &n, 4);

    size_t curr = 8;

    for (const string &s: cmd) {
        uint32_t p = (uint32_t)s.size();
        memcpy(&write_buf[curr], &p, 4);
        memcpy(&write_buf[curr + 4], s.data(), s.size());
        curr += (4 + s.size());
    }
    return write_all(fd, write_buf, 4 + len);
}

static int32_t read_full(int fd, char* buf, size_t n) {
    while (n > 0) {
        ssize_t rv = read(fd, buf, n);
        if (rv <= 0) {
            return -1;
        }
        assert((size_t)rv <= n);
        n -= (size_t)rv;
        buf += rv;
    }
    return 0;
}

static int32_t read_res(int fd) {
    char read_buf[PROTOCOL_REQ_LEN + MAX_MSG + 1];
    errno = 0;

    int32_t err = read_full(fd, read_buf, PROTOCOL_REQ_LEN);
    if (err) {
        if (errno == 0) {
            cout << "EOF\n";
        } else {
            cout << "read() error\n";
        }
        return err;
    }

    uint32_t len = 0;
    uint32_t rescode = 0;
    memcpy(&len, read_buf, PROTOCOL_REQ_LEN);
    if (len > MAX_MSG) {
        cout << "too long\n";
        return -1;
    } else if (len < 4) {
        cout << "bad response\n";
        return -1;
    }

    err = read_full(fd, &read_buf[PROTOCOL_REQ_LEN], len);
    if (err) {
        cout << "read() error\n";
        return err;
    }

    memcpy(&rescode, &read_buf[4], 4);
    printf("server response: [%u] %.*s\n", rescode, len - 4, &read_buf[8]);
    return 0;
}

int main(int argc, char** argv) {
    int fd = initialize_client();

    vector<string> cmd;

    for (int i = 1; i < argc; i++) {
        cmd.push_back(argv[i]);
    }

    int32_t err = send_req(fd, cmd);
    if (err) {
        cout << "Error on request\n";
        goto L_DONE;
    }


    err = read_res(fd);
    if (err) {
        cout << "Error on reading response\n";
        goto L_DONE;
    }
    

L_DONE:
    close(fd);
    return 0;
}
