#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cassert>
#include <errno.h>

using namespace std;

#define PORT 8080
const size_t k_max_msg = 4096;

// Function to read exactly 'n' bytes from the socket
static int32_t read_full(int fd, char *buf, size_t n)
{
    while (n > 0)
    {
        ssize_t rv = read(fd, buf, n);
        if (rv < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            else
            {
                return -1; // Error
            }
        }
        else if (rv == 0)
        {
            return 0; // EOF: remote peer closed connection
        }
        assert((size_t)rv <= n);
        n -= (size_t)rv;
        buf += rv;
    }
    return 0;
}

// Function to write exactly 'n' bytes to the socket
static int32_t write_all(int fd, const char *buf, size_t n)
{
    while (n > 0)
    {
        ssize_t rv = write(fd, buf, n);
        if (rv <= 0)
        {
            return -1; // Error
        }
        assert((size_t)rv <= n);
        n -= (size_t)rv;
        buf += rv;
    }
    return 0;
}

// Function to send a Redis-style response
static int32_t send_response(int fd, const char *msg)
{
    uint32_t len = (uint32_t)strlen(msg);
    if (len > k_max_msg)
    {
        cout << "Response too long" << endl;
        return -1;
    }
    char wbuf[4 + k_max_msg + 2];                            // Buffer for length prefix, message, and \r\n
    snprintf(wbuf, sizeof(wbuf), "$%u\r\n%s\r\n", len, msg); // Format: $<len>\r\n<msg>\r\n
    return write_all(fd, wbuf, strlen(wbuf));
}

// Function to handle one Redis-style request
static int32_t handle_request(int connfd)
{
    char rbuf[4 + k_max_msg + 1];
    errno = 0;

    // Step 1: Read the '$' sign
    int32_t err = read_full(connfd, rbuf, 1);
    if (err || rbuf[0] != '$')
    {
        cout << "Invalid protocol format: missing '$' sign" << endl;
        return -1;
    }

    // Step 2: Read the length prefix (variable-length decimal number)
    char len_buf[32];
    size_t i = 0;
    while (i < sizeof(len_buf) - 1)
    {
        err = read_full(connfd, &len_buf[i], 1);
        if (err)
        {
            perror("Failed to read length prefix");
            return err;
        }
        if (len_buf[i] == '\r')
        {
            len_buf[i] = '\0'; // Null-terminate the length string
            break;
        }
        i++;
    }

    // Step 3: Read the newline after the length prefix
    char nl;
    err = read_full(connfd, &nl, 1);
    if (err || nl != '\n')
    {
        perror("Invalid Redis protocol: missing newline after length");
        return -1;
    }

    // Convert length to integer
    uint32_t len = atoi(len_buf);
    if (len == 0 || len > k_max_msg)
    {
        cout << "Invalid message length" << endl;
        return -1;
    }

    // Step 4: Read the actual message payload
    err = read_full(connfd, rbuf, len);
    if (err)
    {
        perror("Failed to read message");
        return err;
    }
    rbuf[len] = '\0'; // Null-terminate the message
    printf("Client says: %s\n", rbuf);

    // Step 5: Read the trailing \r\n
    err = read_full(connfd, rbuf, 2);
    if (err || rbuf[0] != '\r' || rbuf[1] != '\n')
    {
        perror("Invalid Redis protocol: missing trailing \\r\\n");
        return -1;
    }

    // Step 6: Send a Redis-style response
    const char reply[] = "Hello from server";
    return send_response(connfd, reply);
}

int main()
{
    int server_fd, new_socket;
    int opt = 1;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    // Create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0)
    {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    // Set socket options
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    address.sin_family = AF_INET;
    address.sin_port = htons(PORT);
    address.sin_addr.s_addr = INADDR_ANY;

    // Bind socket to address
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_fd, SOMAXCONN) < 0)
    {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    cout << "Server listening on port " << PORT << endl;

    // Accept multiple connections
    while (true)
    {
        new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
        if (new_socket < 0)
        {
            perror("Accept failed");
            continue;
        }

        cout << "New client connected" << endl;

        // Handle multiple requests in a single connection
        while (true)
        {
            int32_t err = handle_request(new_socket);
            if (err)
            {
                cout << "Closing connection with client" << endl;
                break; // Break on error or EOF
            }
        }

        close(new_socket);
    }

    return 0;
}
