#include <iostream>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 8080
#define k_max_msg 4096 // Maximum message length

using namespace std;

// Function to write all bytes to the socket
static int32_t write_all(int fd, const char *buf, size_t n)
{
    while (n > 0)
    {
        ssize_t rv = write(fd, buf, n);
        if (rv <= 0)
        {
            return -1;
        }
        n -= rv;
        buf += rv;
    }
    return 0;
}

// Function to read exactly 'n' bytes from the socket
static int32_t read_full(int fd, char *buf, size_t n)
{
    while (n > 0)
    {
        ssize_t rv = read(fd, buf, n);
        if (rv <= 0)
        {
            return -1;
        }
        n -= rv;
        buf += rv;
    }
    return 0;
}

// function to send a query to the server and read its response
static int32_t query(int fd, const char *text)
{
    uint32_t len = (uint32_t)strlen(text);
    if (len > k_max_msg)
    {
        cout << "Message too long" << endl;
        return -1;
    }

    // prepare the request with 4-byte length header + message
    char wbuf[4 + k_max_msg];
    memcpy(wbuf, &len, 4);       // Copy 4-byte length (assuming little-endian)
    memcpy(&wbuf[4], text, len); // cpoy the message after the header

    // send the request
    if (int32_t err = write_all(fd, wbuf, 4 + len))
    {
        perror("write_all failed");
        return err;
    }

    // read the 4-byte length header of the response
    char rbuf[4 + k_max_msg + 1]; // Buffer to store response
    int32_t err = read_full(fd, rbuf, 4);
    if (err)
    {
        perror("read_full failed");
        return err;
    }

    // Extract the length of the response message
    memcpy(&len, rbuf, 4); // Assume little-endian
    if (len > k_max_msg)
    {
        cout << "Response too long" << endl;
        return -1;
    }

    // Read the response payload
    err = read_full(fd, &rbuf[4], len);
    if (err)
    {
        perror("read_full failed");
        return err;
    }

    // Null-terminator the response and print it
    rbuf[4 + len] = '\0';
    printf("Server says: %s\n", &rbuf[4]);

    return 0;
}

int main()
{
    int client_fd, status;
    struct sockaddr_in server_addr, local_addr, remote_addr;
    socklen_t addr_len = sizeof(local_addr);

    // creating socket
    client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_fd < 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Specify local address using bind()
    // struct sockaddr_in local_addr = {};
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(0);                      // OS picks the port
    local_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); // Use localhost

    if (bind(client_fd, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Set up server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    // Connect to server
    if (connect(client_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("connect failed");
        return -1;
    }
    printf("Connected to the server\n");

    // Get local address using getsockname()
    if (getsockname(client_fd, (struct sockaddr *)&local_addr, &addr_len) == 0)
    {
        printf("Local IP: %s, Port: %d\n",
               inet_ntoa(local_addr.sin_addr),
               ntohs(local_addr.sin_port));
    }

    // Get server (remote) address using getpeername()
    if (getpeername(client_fd, (struct sockaddr *)&remote_addr, &addr_len) == 0)
    {
        printf("Connected to Server - IP: %s, Port: %d\n",
               inet_ntoa(remote_addr.sin_addr),
               ntohs(remote_addr.sin_port));
    }

    // Send multiple queries to the server
    int32_t err = query(client_fd, "hello1");
    if (err)
        goto L_DONE;

    err = query(client_fd, "hello2");
    if (err)
        goto L_DONE;

    err = query(client_fd, "hello3");
    if (err)
        goto L_DONE;

L_DONE:

    // Closing the socket
    close(client_fd);

    return 0;
}