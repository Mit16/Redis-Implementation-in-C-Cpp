#include <iostream>
#include <cassert>
#include <vector>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <cstddef>
#include <map>

using namespace std;

#define PORT 8080
const size_t k_max_msg = 32 << 20;    // 32 MB
const size_t k_max_args = 200 * 1000; // Maximum number of arguments in a request

// Connection state
struct Conn
{
    int fd = -1;
    bool want_read = false;
    bool want_write = false;
    bool want_close = false;
    vector<uint8_t> incoming; // Data to be parsed by the application
    vector<uint8_t> outgoing; // Responses generated by the application
};

// Logging and error handling
static void msg(const char *msg)
{
    cerr << msg << endl;
}

static void msg_errno(const char *msg)
{
    cerr << "[errno:" << errno << "] " << msg << endl;
}

static void die(const char *msg)
{
    msg_errno(msg);
    exit(EXIT_FAILURE);
}

// Append data to the buffer
static void buf_append(vector<uint8_t> &buf, const uint8_t *data, size_t len)
{
    buf.insert(buf.end(), data, data + len);
}

// Remove data from the front of the buffer
static void buf_consume(vector<uint8_t> &buf, size_t n)
{
    buf.erase(buf.begin(), buf.begin() + n);
}

// Set a file descriptor to non-blocking mode
static void fd_set_nb(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0)
    {
        die("fcntl(F_GETFL) failed");
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
    {
        die("fcntl(F_SETFL) failed");
    }
}

// Handle new connections
static Conn *handle_accept(int fd)
{
    struct sockaddr_in client_addr = {};
    socklen_t socklen = sizeof(client_addr);
    int connfd = accept(fd, (struct sockaddr *)&client_addr, &socklen);
    if (connfd < 0)
    {
        msg_errno("accept() failed");
        return nullptr;
    }

    // Set the new connection to non-blocking mode
    fd_set_nb(connfd);

    // Create a new connection object
    Conn *conn = new Conn();
    conn->fd = connfd;
    conn->want_read = true; // Start by reading the first request
    return conn;
}

// Helper functions for parsing requests
static bool read_u32(const uint8_t *&cur, const uint8_t *end, uint32_t &out)
{
    if (cur + 4 > end)
    {
        return false;
    }
    memcpy(&out, cur, 4);
    cur += 4;
    return true;
}

static bool read_str(const uint8_t *&cur, const uint8_t *end, size_t n, string &out)
{
    if (cur + n > end)
    {
        return false;
    }
    out.assign(cur, cur + n);
    cur += n;
    return true;
}

// Parse a Redis-like request into a list of strings
static int32_t parse_req(const uint8_t *data, size_t size, vector<string> &out)
{
    const uint8_t *end = data + size;
    uint32_t nstr = 0;
    if (!read_u32(data, end, nstr))
    {
        return -1;
    }
    if (nstr > k_max_args)
    {
        return -1; // Safety limit
    }

    while (out.size() < nstr)
    {
        uint32_t len = 0;
        if (!read_u32(data, end, len))
        {
            return -1;
        }
        out.push_back(string());
        if (!read_str(data, end, len, out.back()))
        {
            return -1;
        }
    }
    if (data != end)
    {
        return -1; // Trailing garbage
    }
    return 0;
}

// Response status codes
enum
{
    RES_OK = 0,
    RES_ERR = 1, // Error
    RES_NX = 2,  // Key not found
};

// Response structure
struct Response
{
    uint32_t status = 0;
    vector<uint8_t> data;
};

// Global key-value store (placeholder)
static map<string, string> g_data;

// Process a command and generate a response
static void do_request(vector<string> &cmd, Response &out)
{
    if (cmd.size() == 2 && cmd[0] == "get")
    {
        auto it = g_data.find(cmd[1]);
        if (it == g_data.end())
        {
            out.status = RES_NX; // Key not found
            out.data.assign("Key not found", "Key not found" + 13);
            return;
        }
        const string &val = it->second;
        out.data.assign(val.begin(), val.end());
    }
    else if (cmd.size() == 3 && cmd[0] == "set")
    {
        g_data[cmd[1]] = cmd[2];
        out.status = RES_OK;
        out.data.assign("OK", "OK" + 2);
    }
    else if (cmd.size() == 2 && cmd[0] == "del")
    {
        if (g_data.erase(cmd[1]) > 0)
        {
            out.status = RES_OK;
            out.data.assign("OK", "OK" + 2);
        }
        else
        {
            out.status = RES_NX; // Key not found
            out.data.assign("Key not found", "Key not found" + 13);
        }
    }
    else
    {
        out.status = RES_ERR; // Unrecognized command
        out.data.assign("Error: Unrecognized command", "Error: Unrecognized command" + 27);
    }
}

// Serialize the response into the outgoing buffer
static void make_response(const Response &resp, vector<uint8_t> &out)
{
    uint32_t resp_len = 4 + (uint32_t)resp.data.size();
    buf_append(out, (const uint8_t *)&resp_len, 4);
    buf_append(out, (const uint8_t *)&resp.status, 4);
    buf_append(out, resp.data.data(), resp.data.size());
}

// Process one request if there is enough data
static bool try_one_request(Conn *conn)
{
    // Try to parse the protocol: message header
    if (conn->incoming.size() < 4)
    {
        return false; // Need more data
    }
    uint32_t len = 0;
    memcpy(&len, conn->incoming.data(), 4);
    if (len > k_max_msg)
    {
        msg("too long");
        conn->want_close = true;
        return false; // Want close
    }
    // Message body
    if (4 + len > conn->incoming.size())
    {
        return false; // Need more data
    }
    const uint8_t *request = &conn->incoming[4];

    // Parse the request into a list of strings
    vector<string> cmd;
    if (parse_req(request, len, cmd) < 0)
    {
        msg("bad request");
        conn->want_close = true;
        return false; // Want close
    }

    // Process the command and generate a response
    Response resp;
    do_request(cmd, resp);
    make_response(resp, conn->outgoing);

    // Remove the processed message from the incoming buffer
    buf_consume(conn->incoming, 4 + len);
    return true; // Success
}

// Handle read events
static void handle_read(Conn *conn)
{
    // Perform a non-blocking read
    uint8_t buf[4096];
    ssize_t rv = read(conn->fd, buf, sizeof(buf));
    if (rv < 0 && errno == EAGAIN)
    {
        return; // Not ready yet
    }
    if (rv < 0)
    {
        msg_errno("read() failed");
        conn->want_close = true;
        return;
    }
    if (rv == 0)
    {
        msg("Client closed connection");
        conn->want_close = true;
        return;
    }

    // Append the new data to the incoming buffer
    buf_append(conn->incoming, buf, rv);

    // Process requests from the buffer
    while (try_one_request(conn))
    {
    }

    // Update the connection state
    if (conn->outgoing.size() > 0)
    {
        conn->want_read = false;
        conn->want_write = true;
    }
}

// Handle write events
static void handle_write(Conn *conn)
{
    // Perform a non-blocking write
    ssize_t rv = write(conn->fd, conn->outgoing.data(), conn->outgoing.size());
    if (rv < 0 && errno == EAGAIN)
    {
        return; // Not ready yet
    }
    if (rv < 0)
    {
        msg_errno("write() failed");
        conn->want_close = true;
        return;
    }

    // Remove the written data from the outgoing buffer
    buf_consume(conn->outgoing, rv);

    // Update the connection state
    if (conn->outgoing.size() == 0)
    {
        conn->want_write = false;
        conn->want_read = true;
    }
}

int main()
{
    // Create the listening socket
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
    {
        die("socket() failed");
    }

    // Set socket options
    int val = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    // Bind the socket
    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        die("bind() failed");
    }

    // Set the listening socket to non-blocking mode
    fd_set_nb(fd);

    // Start listening
    if (listen(fd, SOMAXCONN) < 0)
    {
        die("listen() failed");
    }

    cout << "Server listening on port " << PORT << endl;

    // Map of all client connections, keyed by file descriptor
    vector<Conn *> fd2conn;

    // Event loop
    while (true)
    {
        // Prepare the arguments for poll()
        vector<pollfd> poll_args;
        pollfd pfd = {fd, POLLIN, 0};
        poll_args.push_back(pfd);

        // Add connection sockets to the poll list
        for (Conn *conn : fd2conn)
        {
            if (!conn)
                continue;
            pollfd pfd = {conn->fd, POLLERR, 0};
            if (conn->want_read)
                pfd.events |= POLLIN;
            if (conn->want_write)
                pfd.events |= POLLOUT;
            poll_args.push_back(pfd);
        }

        // Wait for socket readiness
        int rv = poll(poll_args.data(), poll_args.size(), -1);
        if (rv < 0 && errno == EINTR)
        {
            continue; // Retry on interrupt
        }
        if (rv < 0)
        {
            die("poll() failed");
        }

        // Handle the listening socket
        if (poll_args[0].revents)
        {
            if (Conn *conn = handle_accept(fd))
            {
                if (fd2conn.size() <= (size_t)conn->fd)
                {
                    fd2conn.resize(conn->fd + 1);
                }
                fd2conn[conn->fd] = conn;
            }
        }

        // Handle connection sockets
        for (size_t i = 1; i < poll_args.size(); ++i)
        {
            uint32_t ready = poll_args[i].revents;
            Conn *conn = fd2conn[poll_args[i].fd];
            if (ready & POLLIN)
                handle_read(conn);
            if (ready & POLLOUT)
                handle_write(conn);
            if ((ready & POLLERR) || conn->want_close)
            {
                close(conn->fd);
                fd2conn[conn->fd] = nullptr;
                delete conn;
            }
        }
    }

    return 0;
}