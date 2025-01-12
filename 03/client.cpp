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
        if (rv < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            else
            {
                return -1;
            }
        }
        else if (rv == 0)
        {
            return 0; // EOF: remote peer closed connection
        }
        n -= rv;
        buf += rv;
    }
    return 0;
}

// Function to send a Redis-style request
static int32_t send_request(int fd, const char *text)
{
    uint32_t len = (uint32_t)strlen(text);
    if (len == 0 || len > k_max_msg)
    {
        cout << "Message length invalid" << endl;
        return -1;
    }

    // Format the request in Redis protocol: $<len>\r\n<text>\r\n
    char wbuf[64 + k_max_msg]; // Increased buffer size to handle large inputs
    int header_len = snprintf(wbuf, sizeof(wbuf), "$%u\r\n", len);
    if (header_len < 0 || header_len >= sizeof(wbuf))
    {
        cout << "Failed to format header" << endl;
        return -1;
    }

    // Copy the text and append \r\n at the end
    memcpy(wbuf + header_len, text, len);
    memcpy(wbuf + header_len + len, "\r\n", 2);

    // Send the entire formatted request
    size_t total_len = header_len + len + 2; // Total length of the request
    if (int32_t err = write_all(fd, wbuf, total_len))
    {
        perror("write_all failed");
        return err;
    }

    return 0;
}

// Function to read a Redis-style response
static int32_t read_response(int fd)
{
    char rbuf[4 + k_max_msg + 1]; // Buffer to store response
    errno = 0;

    // Step 1: Read the '$' sign
    int32_t err = read_full(fd, rbuf, 1);
    if (err || rbuf[0] != '$')
    {
        cout << "Invalid Redis protocol: missing '$' sign" << endl;
        return -1;
    }

    // Step 2: Read the length prefix (variable-length decimal number)
    char len_buf[32];
    size_t i = 0;
    while (i < sizeof(len_buf) - 1)
    {
        err = read_full(fd, &len_buf[i], 1);
        if (err)
        {
            perror("read_full failed");
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
    err = read_full(fd, &nl, 1);
    if (err || nl != '\n')
    {
        perror("Invalid Redis protocol: missing newline after length");
        return -1;
    }

    // Convert length to integer
    uint32_t len = atoi(len_buf);
    if (len == 0 || len > k_max_msg)
    {
        cout << "Invalid or too long message length" << endl;
        return -1;
    }

    // Step 4: Read the actual bulk string message
    err = read_full(fd, rbuf, len);
    if (err)
    {
        perror("read_full failed");
        return err;
    }
    rbuf[len] = '\0'; // Null-terminate the message
    printf("Server says: %s\n", rbuf);

    // Step 5: Read the trailing \r\n
    err = read_full(fd, rbuf, 2);
    if (err || rbuf[0] != '\r' || rbuf[1] != '\n')
    {
        perror("Invalid Redis protocol: missing trailing \\r\\n");
        return -1;
    }

    return 0;
}

// Function to send a query to the server and read its response
static int32_t query(int fd, const char *text)
{
    // Send the request in Redis protocol format
    if (int32_t err = send_request(fd, text))
    {
        return err;
    }

    // Read the response in Redis protocol format
    return read_response(fd);
}

int main()
{
    int client_fd, status;
    struct sockaddr_in server_addr, local_addr, remote_addr;
    socklen_t addr_len = sizeof(local_addr);

    // Create socket
    client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_fd < 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Specify local address using bind()
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
    server_addr.sin_port = htons(PORT);
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