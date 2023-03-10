#include "../common/server.h"

// -------------------- operation area --------------------
static map<string, string> g_map;

static uint32_t do_get(const vector<string> &cmd, uint8_t* res, uint32_t* reslen) {
    if (!g_map.count(cmd[1])) {
        return RES_NX;
    }
    string &val = g_map[cmd[1]];
    assert(val.size() <= MAX_ARGS);
    memcpy(res, val.data(), val.size());
    *reslen = (uint32_t)val.size();
    return RES_OK;
}

static uint32_t do_set(const vector<string> &cmd, uint8_t* res, uint32_t* reslen) {
    (void)res;
    (void)reslen;
    g_map[cmd[1]] = cmd[2];
    return RES_OK;
}

static uint32_t do_del(const vector<string> &cmd, uint8_t* res, uint32_t* reslen) {
    (void)res;
    (void)reslen;
    g_map.erase(cmd[1]);
    return RES_OK;
}
// --------------------------------------------------------

// Set fd to non-blocking mode
static void fd_set_nb(int fd) {
    errno = 0;
    int flags = fcntl(fd, F_GETFL, 0);
    if (errno) {
        println("Error on fcntl call");
        return;
    }

    errno = 0;
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    if (errno) {
        println("Error on fcntl call");
    }
}

static int32_t parse_req(const uint8_t* data, size_t len, vector<string> &out) {
    if (len < PROTO_STR_NUMBER) {
        return -1;
    }

    /**
     * Structure of `data`
     * +------+-----+------+-----+------+-----+-----+------+
     * | nstr | len | str1 | len | str2 | ... | len | strn |
     * +------+-----+------+-----+------+-----+-----+------+
    */
    uint32_t num_ops = 0;
    memcpy(&num_ops, &data[0], PROTO_STR_NUMBER);
    if (num_ops > MAX_ARGS) {
        return -1;
    }

    size_t pos = PROTO_STR_NUMBER;
    while (num_ops--) {
        if (pos + PROTO_STR_LEN > len) {
            return -1;
        }
        uint32_t str_len = 0;    // size of each str
        memcpy(&str_len, &data[pos], PROTO_STR_LEN);
        if (pos + PROTO_STR_LEN + str_len > len) {
            return -1;
        }
        out.push_back(string((char*)&data[pos + PROTO_STR_LEN], str_len));
        pos += (PROTO_STR_LEN + str_len);
    }
    if (pos != len) {
        return -1;
    }
    return 0;
}

static void print_cmd(vector<string> cmd) {
    cout << "Executing: ";
    for (string s: cmd) {
        cout << s << ' ';
    }
    cout << '\n';
}

// handle request
static int32_t do_request(const uint8_t* req, uint32_t reqlen, uint32_t* rescode, uint8_t* res, uint32_t* reslen) {
    vector<string> cmd;

    if (parse_req(req, reqlen, cmd) != 0) {
        return println_and_return("Bad request", -1);
    }

    print_cmd(cmd);

    if (cmd.size() == 2 && cmd[0].compare("get") == 0) {
        *rescode = do_get(cmd, res, reslen);
    } else if (cmd.size() == 3 && cmd[0].compare("set") == 0) {
        *rescode = do_set(cmd, res, reslen);
    } else if (cmd.size() == 2 && cmd[0].compare("del") == 0) {
        *rescode = do_del(cmd, res, reslen);
    } else {
        *rescode = RES_ERR;
        const char* msg = "Unkown command";
        strcpy((char*) res, msg);
        *reslen = strlen(msg);
    }

    return 0;
}

// handles the incoming data
static bool try_one_request(Conn* conn) {
    if (conn->read_buf_size < PROTO_PAYLOAD_LEN) { return false; }

    uint32_t len = 0;
    memcpy(&len, &conn->read_buf[0], PROTO_PAYLOAD_LEN);
    if (len > MAX_MSG) {
        println("Message is too long");
        conn->state = STATE_END;
        return false;
    }

    if (PROTO_PAYLOAD_LEN + len > conn->read_buf_size) { return false; }

    uint32_t rescode = 0;
    uint32_t write_len = 0;
    uint32_t err = do_request(&conn->read_buf[PROTO_PAYLOAD_LEN], len, &rescode, &conn->write_buf[PROTO_PAYLOAD_LEN + PROTO_RES_CODE], &write_len);

    if (err) {
        conn->state = STATE_END;
        return false;
    }

    /**
     * Response structure
     * +-----+---------+
     * | res | data... |
     * +-----+---------+
    */
    write_len += PROTO_RES_CODE;
    memcpy(&conn->write_buf[0], &write_len, PROTO_PAYLOAD_LEN);
    memcpy(&conn->write_buf[PROTO_PAYLOAD_LEN], &rescode, PROTO_RES_CODE);
    conn->write_buf_size = PROTO_PAYLOAD_LEN + write_len;

    size_t remaining = conn->read_buf_size - (PROTO_PAYLOAD_LEN + len);
    if (remaining) {
        memmove(conn->read_buf, &conn->read_buf[PROTO_PAYLOAD_LEN + len], remaining);
    }
    conn->read_buf_size = remaining;

    conn->state = STATE_RES;
    state_res(conn);
    
    return conn->state == STATE_REQ;
}

static bool try_fill_buffer(Conn* conn) {
    assert(conn->read_buf_size < sizeof(conn->read_buf));
    ssize_t rv = 0;
    
    do {
        size_t size_left = sizeof(conn->read_buf) - conn->read_buf_size;
        rv = read(conn->fd, &conn->read_buf[conn->read_buf_size], size_left);
    } while(rv < 0 && errno == EINTR);  // EINTR: interrupted by a signal

    if (rv < 0) {
        if (errno == EAGAIN) {
            return false;
        }

        println("Error on read call");
        conn->state = STATE_END;
        return false;
    }

    if (rv == 0) {
        if (conn->read_buf_size > 0) {
            println("Unexpected EOF");
        } else {
            println("EOF");
        }

        conn->state = STATE_END;
        return false;
    }

    conn->read_buf_size += (size_t) rv;
    assert(conn->read_buf_size <= sizeof(conn->read_buf) - conn->read_buf_size);

    /**
     * try_one_request is called in a loop because of pipelining,
     * which means there can be more than one request in the read buffer.
     */
    while (try_one_request(conn)) {}
    return conn->state == STATE_REQ;
}

static bool try_flush_buffer(Conn* conn) {
    ssize_t rv = 0;
    
    do {
        size_t size_left = conn->write_buf_size - conn->write_buf_sent;
        rv = write(conn->fd, &conn->write_buf[conn->write_buf_sent], size_left);
    } while(rv < 0 && errno == EINTR);

    if (rv < 0) {
        if (errno == EAGAIN) {
            return false;
        }
        println("Error on write call");
        conn->state = STATE_END;
        return false;
    }

    conn->write_buf_sent += (size_t)rv;
    assert(conn->write_buf_sent <= conn->write_buf_size);

    if (conn->write_buf_sent == conn->write_buf_size) {
        // change state back because response was fully sent
        conn->state = STATE_REQ;
        conn->write_buf_sent = 0;
        conn->write_buf_size = 0;
        return false;
    }

    // data left in write_buf. could try to write again
    return true;
}

static void state_req(Conn* conn) {
    while (try_fill_buffer(conn)) {}
}

static void state_res(Conn* conn) {
    while (try_flush_buffer(conn)) {}
}

// State machine for client connections
static void connection_io(Conn* conn) {
    if (conn->state == STATE_REQ) {
        state_req(conn);
    } else if (conn->state == STATE_RES) {
        state_res(conn);
    } else {
        assert(0);
    }
}

static void conn_put(vector<Conn*>&fd2conn, struct Conn* conn) {
    if (fd2conn.size() <= (size_t)conn->fd) {
        fd2conn.resize(conn->fd+1);
    }
    fd2conn[conn->fd] = conn;
}

static int32_t accept_new_conn(vector<Conn*> &fd2conn, int fd) {
    struct sockaddr_in client_addr = {};
    socklen_t socklen = sizeof(client_addr);
    int connfd;

    if ((connfd = accept(fd, (struct sockaddr*)&client_addr, &socklen)) < 0) {
        return println_and_return("Error on accept call", -1);
    }
    fd_set_nb(connfd);
    struct Conn* conn = (struct Conn*) malloc(sizeof(struct Conn));
    if (!conn) {
        close(connfd);
        return -1;
    }

    conn->fd = connfd;
    conn->state = STATE_REQ;
    conn->read_buf_size = 0;
    conn->write_buf_size = 0;
    conn->write_buf_sent = 0;
    conn_put(fd2conn, conn);
    return 0;
}

int main() {
    int fd = initialize_server();

    vector<Conn *> fd2conn;
    fd_set_nb(fd);

    // EVENT LOOP
    vector<struct pollfd> poll_args;
    while (true) {
        poll_args.clear();
        
        struct pollfd pfd = {fd, POLLIN, 0};
        poll_args.push_back(pfd);

        for (Conn* conn: fd2conn) {
            if (!conn) {
                continue;
            }
            struct pollfd pfd = {};
            pfd.fd = conn->fd;
            pfd.events = conn->state == STATE_REQ ? POLLIN : POLLOUT;
            pfd.revents = pfd.events | POLLERR;
            poll_args.push_back(pfd);
        }

        // After the return of poll, we are notified by which fd are ready for reading/writing and act accordingly
        if (poll(poll_args.data(), (nfds_t)poll_args.size(), 1000) < 0) {
            println("Error on poll call");
            exit(1);
        }

        for (size_t i = 1; i < poll_args.size(); i++) {
            if (poll_args[i].revents) {
                Conn* conn = fd2conn[poll_args[i].fd];
                connection_io(conn);

                if (conn->state == STATE_END) {
                    fd2conn[conn->fd] = NULL;
                    close(conn->fd);
                    free(conn);
                }
            }
        }

        if (poll_args[0].revents) {
            accept_new_conn(fd2conn, fd);
        }
    }
}
