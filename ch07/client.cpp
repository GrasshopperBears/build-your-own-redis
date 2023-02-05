#include "../common/client.h"

static int32_t send_req(int fd, const vector<string> &cmd) {
    uint32_t len = PROTO_PAYLOAD_LEN;
    for (const string &s: cmd) {
        len += (PROTO_STR_LEN + s.size());
    }

    if (len > MAX_ARGS) { return -1; }

    char write_buf[PROTO_PAYLOAD_LEN + MAX_ARGS];
    uint32_t num_ops = cmd.size();

    memcpy(&write_buf[0], &len, PROTO_PAYLOAD_LEN);
    memcpy(&write_buf[PROTO_PAYLOAD_LEN], &num_ops, PROTO_STR_NUMBER);

    size_t curr = PROTO_PAYLOAD_LEN + PROTO_STR_NUMBER;

    for (const string &s: cmd) {
        uint32_t p = (uint32_t)s.size();
        memcpy(&write_buf[curr], &p, PROTO_STR_LEN);
        memcpy(&write_buf[curr + PROTO_STR_LEN], s.data(), s.size());
        curr += (PROTO_STR_LEN + s.size());
    }
    return write_all(fd, write_buf, 4 + len);
}

static int32_t read_res(int fd) {
    char read_buf[PROTO_PAYLOAD_LEN + MAX_MSG + 1];
    errno = 0;

    int32_t err = read_full(fd, read_buf, PROTO_PAYLOAD_LEN);
    if (err) {
        if (errno == 0) {
            println("EOF");
        } else {
            println("Error on read call");
        }
        return err;
    }

    uint32_t len = 0;
    uint32_t rescode = 0;
    memcpy(&len, read_buf, PROTO_PAYLOAD_LEN);
    if (len > MAX_MSG) {
        return println_and_return("too long", -1);
    } else if (len < 4) {
        return println_and_return("bad response", -1);
    }

    err = read_full(fd, &read_buf[PROTO_PAYLOAD_LEN], len);
    if (err) {
        return println_and_return("Error at read call", err);
    }

    memcpy(&rescode, &read_buf[PROTO_PAYLOAD_LEN], PROTO_RES_CODE);
    printf("server response: [%s] %.*s\n", rescode2str(rescode), len - 4, &read_buf[8]);
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
        println("Error on request");
        goto L_DONE;
    }


    err = read_res(fd);
    if (err) {
        println("Error on reading response");
        goto L_DONE;
    }
    
L_DONE:
    close(fd);
    return 0;
}
