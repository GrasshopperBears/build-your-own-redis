#include "../buildRedisCommon.h"

enum {
    STATE_REQ = 0,  // reading request
    STATE_RES = 1,  // sending response
    STATE_END = 2,
};

struct Conn {
    int fd = -1;
    uint32_t state = 0;

    size_t read_buf_size = 0;
    uint8_t read_buf[PROTOCOL_REQ_LEN + MAX_MSG];
    
    size_t write_buf_size = 0;
    size_t write_buf_sent = 0;
    uint8_t write_buf[PROTOCOL_REQ_LEN + MAX_MSG];
};

static void state_req(Conn* conn);
static void state_res(Conn* conn);

// Set fd to non-blocking mode
static void fd_set_nb(int fd) {
    errno = 0;
    int flags = fcntl(fd, F_GETFL, 0);
    if (errno) {
        cout << "Error on fcntl call\n";
        return;
    }

    errno = 0;
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    if (errno) {
        cout << "Error on fcntl call\n";
    }
}

// handles the incoming data
static bool try_one_request(Conn* conn) {
    if (conn->read_buf_size < PROTOCOL_REQ_LEN) {
        // Not enough space in the buffer. Will retry in the next iteration
        return false;
    }

    uint32_t len = 0;
    memcpy(&len, &conn->read_buf[0], PROTOCOL_REQ_LEN);
    if (len > MAX_MSG) {
        cout << "Message is too long\n";
        conn->state = STATE_END;
        return false;
    }

    if (PROTOCOL_REQ_LEN + len > conn->read_buf_size) {
        // same as the part at the beginning of this function
        return false;
    }

    printf("Data from client: %.*s\n", len, &conn->read_buf[PROTOCOL_REQ_LEN]);

    // echo response
    memcpy(&conn->write_buf[0], &len, PROTOCOL_REQ_LEN);
    memcpy(&conn->write_buf[PROTOCOL_REQ_LEN], &conn->read_buf[PROTOCOL_REQ_LEN], len);
    conn->write_buf_size = PROTOCOL_REQ_LEN + len;

    /**
     * Removing request from buffer.
     * However, it should be improved because frequent memmove drops efficiency
     */
    size_t remaining = conn->read_buf_size - (PROTOCOL_REQ_LEN + len);
    if (remaining) {
        memmove(conn->read_buf, &conn->read_buf[PROTOCOL_REQ_LEN + len], remaining);
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

        cout << "Error on read call\n";
        conn->state = STATE_END;
        return false;
    }

    if (rv == 0) {
        if (conn->read_buf_size > 0) {
        cout << "Unexpected EOF\n";
        } else {
        cout << "EOF\n";
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
        cout << "Error on write call\n";
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
        cout << "Error on accept call\n";
        return -1;
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
    int fd;
    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        cout << "Error on socket call\n";
        exit(1);
    }

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = ntohs(8080);
    addr.sin_addr.s_addr = ntohl(0);

    if (bind(fd, (const sockaddr*)&addr, sizeof(addr)) > 0) {
        cout << "Error on bind call\n";
        exit(1);
    }

    if (listen(fd, SOMAXCONN)) {
        cout << "Error on listen call\n";
        exit(1);
    }

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
            cout << "Error on poll call\n";
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
