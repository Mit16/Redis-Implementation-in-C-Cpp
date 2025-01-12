#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cassert>

using namespace std;

#define PORT 8080
#define BUFFER_SIZE 1024
const size_t k_max_msg = 4096;

// Function to read exactly 'n' bytes from the socket
static int32_t read_full(int fd, char *buf, size_t n)
{
    while (n > 0)
    {
        ssize_t rv = read(fd, buf, n);
        if (rv <= 0)
        {
            return -1; // error or EOF
        }
        assert((size_t)rv <= n);
        n -= (size_t)rv;
        buf += rv;
    }
    return 0;
}

// Function to write exactly 'n' bytes to the socket
static int32_t write_all(int fd, char *buf, size_t n)
{
    while (n > 0)
    {
        ssize_t rv = write(fd, buf, n);
        if (rv <= 0)
        {
            return -1;
        }
        assert((size_t)rv <= n);
        n -= (size_t)rv;
        buf += rv;
    }
    return 0;
}

static int32_t one_request(int connfd)
{
    char rbuf[4 + k_max_msg + 1];
    errno = 0;

    // step 1: Read 4-byte length header
    int32_t err = read_full(connfd, rbuf, 4);
    if (err)
    {
        if (errno == 0)
        {
            cout << "EOF" << endl;
        }
        else
        {
            perror("read error");
        }
        return err;
    }

    // extract message length
    uint32_t len = 0;
    memcpy(&len, rbuf, 4); // Assume little-endian format
    if (len > k_max_msg)
    {
        cout << "Message too long" << endl;
        return -1;
    }

    // step 2: read the message payload
    err = read_full(connfd, &rbuf[4], len);
    if (err)
    {
        perror("read error");
        return err;
    }

    // Null-terminating and print the message
    rbuf[4 + len] = '\0';
    printf("Client says: %s\n", &rbuf[4]);

    // step 3:Create a response message
    const char reply[] = "Hello from server";
    char wbuf[4 + sizeof(reply)];
    len = (uint32_t)strlen(reply);
    memcpy(wbuf, &len, 4);
    memcpy(&wbuf[4], reply, len);

    // step 4: send response
    return write_all(connfd, wbuf, 4 + len);
}

int main()
{
    int server_fd, new_socket;
    int opt = 1;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    // create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // set socket options
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    address.sin_family = AF_INET;
    address.sin_port = htons(PORT);
    address.sin_addr.s_addr = INADDR_ANY;

    // bind socket to address
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_fd, SOMAXCONN))
    {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    // Accept multiple connections
    while (true)
    {
        // accept
        new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
        if (new_socket < 0)
        {
            continue; // error, try again
        }

        // Handle multiple requests in a single connection
        while (true)
        {
            int32_t err = one_request(new_socket);
            if (err)
            {
                break; // Break on error or EOF(End of file)
            }
        }

        close(new_socket);
    }

    return 0;
}