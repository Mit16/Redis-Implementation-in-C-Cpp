#include <iostream>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>
#include <cstddef>

#define PORT 8080
const size_t k_max_msg = 32 << 20; // 32 MB

using namespace std;

// Logging and error handling
static void msg(const char *msg)
{
    cerr << msg << endl;
}

static void die(const char *msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

// Write all data to the socket
static int32_t write_all(int fd, const void *buf, size_t n)
{
    const uint8_t *data = static_cast<const uint8_t *>(buf);
    while (n > 0)
    {
        ssize_t rv = write(fd, data, n);
        if (rv <= 0)
            return -1;
        data += rv;
        n -= rv;
    }
    return 0;
}

// Read all data from the socket
static int32_t read_full(int fd, void *buf, size_t n)
{
    uint8_t *data = static_cast<uint8_t *>(buf);
    while (n > 0)
    {
        ssize_t rv = read(fd, data, n);
        if (rv < 0)
        {
            if (errno == EINTR)
                continue;
            return -1;
        }
        if (rv == 0)
            return 0; // EOF
        data += rv;
        n -= rv;
    }
    return 0;
}

// Function to send a Redis-style request
// static int32_t send_request(int fd, const char *text)
// {
//     uint32_t len = (uint32_t)strlen(text);
//     if (len == 0 || len > k_max_msg)
//     {
//         msg("Message length invalid");
//         return -1;
//     }

//     // Format the request in Redis protocol: $<len>\r\n<text>\r\n
//     char wbuf[64 + k_max_msg]; // Increased buffer size to handle large inputs
//     int header_len = snprintf(wbuf, sizeof(wbuf), "$%u\r\n", len);
//     if (header_len < 0 || header_len >= sizeof(wbuf))
//     {
//         msg("Failed to format header");
//         return -1;
//     }

//     // Copy the text and append \r\n at the end
//     memcpy(wbuf + header_len, text, len);
//     memcpy(wbuf + header_len + len, "\r\n", 2);

//     // Send the entire formatted request
//     size_t total_len = header_len + len + 2; // Total length of the request
//     if (int32_t err = write_all(fd, wbuf, total_len))
//     {
//         msg("write_all failed");
//         return err;
//     }

//     return 0;
// }

// // Function to read a Redis-style response
// static int32_t read_response(int fd)
// {
//     char rbuf[4 + k_max_msg + 1]; // Buffer to store response
//     errno = 0;

//     // Step 1: Read the '$' sign
//     int32_t err = read_full(fd, rbuf, 1);
//     if (err || rbuf[0] != '$')
//     {
//         msg("Invalid Redis protocol: missing '$' sign");
//         return -1;
//     }

//     // Step 2: Read the length prefix (variable-length decimal number)
//     char len_buf[32];
//     size_t i = 0;
//     while (i < sizeof(len_buf) - 1)
//     {
//         err = read_full(fd, &len_buf[i], 1);
//         if (err)
//         {
//             msg("read_full failed");
//             return err;
//         }
//         if (len_buf[i] == '\r')
//         {
//             len_buf[i] = '\0'; // Null-terminate the length string
//             break;
//         }
//         i++;
//     }

//     // Step 3: Read the newline after the length prefix
//     char nl;
//     err = read_full(fd, &nl, 1);
//     if (err || nl != '\n')
//     {
//         msg("Invalid Redis protocol: missing newline after length");
//         return -1;
//     }

//     // Convert length to integer
//     uint32_t len = atoi(len_buf);
//     if (len == 0 || len > k_max_msg)
//     {
//         msg("Invalid or too long message length");
//         return -1;
//     }

//     // Step 4: Read the actual bulk string message
//     err = read_full(fd, rbuf, len);
//     if (err)
//     {
//         msg("read_full failed");
//         return err;
//     }
//     rbuf[len] = '\0'; // Null-terminate the message
//     printf("Server says: %s\n", rbuf);

//     // Step 5: Read the trailing \r\n
//     err = read_full(fd, rbuf, 2);
//     if (err || rbuf[0] != '\r' || rbuf[1] != '\n')
//     {
//         msg("Invalid Redis protocol: missing trailing \\r\\n");
//         return -1;
//     }

//     return 0;
// }

// // Function to send a query to the server and read its response
// static int32_t query(int fd, const char *text)
// {
//     // Send the request in Redis protocol format
//     if (int32_t err = send_request(fd, text))
//     {
//         return err;
//     }

//     // Read the response in Redis protocol format
//     return read_response(fd);
// }
// Send a request to the server

// Send a request to the server
static int32_t send_request(int fd, const vector<string> &cmd)
{
    // Calculate total message size
    uint32_t num_args = cmd.size();
    uint32_t msg_len = 4; // For the number of strings
    for (const string &arg : cmd)
    {
        msg_len += 4 + arg.size(); // 4 bytes for length + string data
    }

    if (msg_len > k_max_msg)
    {
        msg("Request too large");
        return -1;
    }

    // Serialize the request
    vector<uint8_t> buf(4 + msg_len); // 4 bytes for total length
    uint32_t total_len = msg_len;
    memcpy(buf.data(), &total_len, 4);

    uint8_t *p = buf.data() + 4; // Skip total length
    memcpy(p, &num_args, 4);
    p += 4;

    for (const string &arg : cmd)
    {
        uint32_t arg_len = arg.size();
        memcpy(p, &arg_len, 4);
        p += 4;
        memcpy(p, arg.data(), arg.size());
        p += arg.size();
    }

    // Send serialized request
    return write_all(fd, buf.data(), buf.size());
}

// Read and parse the server response
static int32_t read_response(int fd)
{
    // Step 1: Read response length (4 bytes)
    uint32_t len = 0;
    if (read_full(fd, &len, 4) < 0)
    {
        msg("Failed to read response length");
        return -1;
    }

    if (len > k_max_msg)
    {
        msg("Response too large");
        return -1;
    }

    // Step 2: Read the full response
    vector<uint8_t> buf(len);
    if (read_full(fd, buf.data(), len) < 0)
    {
        msg("Failed to read full response");
        return -1;
    }

    // Step 3: Parse the response
    uint32_t status = 0;
    memcpy(&status, buf.data(), 4);

    if (status == 0)
    { // RES_OK
        string data(buf.begin() + 4, buf.end());
        cout << "Response: " << data << endl;
    }
    else if (status == 2)
    { // RES_NX
        cout << "Response: Key not found" << endl;
    }
    else
    { // RES_ERR
        cout << "Response: Error" << endl;
    }

    return 0;
}

// Query the server with a command
static int32_t query(int fd, const vector<string> &cmd)
{
    if (send_request(fd, cmd) < 0)
    {
        msg("Failed to send request");
        return -1;
    }
    return read_response(fd);
}

int main(int argc, char **argv)
{

    int client_fd;
    struct sockaddr_in server_addr, local_addr, remote_addr;
    socklen_t addr_len = sizeof(local_addr);

    // Create socket
    client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_fd < 0)
    {
        die("socket failed");
    }

    // Specify local address using bind()
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(0);                      // OS picks the port
    local_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); // Use localhost

    if (bind(client_fd, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0)
    {
        die("bind failed");
    }

    // Set up server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0)
    {
        die("Invalid address/ Address not supported");
    }

    // Connect to server
    if (connect(client_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        die("connect failed");
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

    // Interactive client loop
    while (true)
    {
        cout << "Enter command: ";
        string input;
        if (!getline(cin, input))
            break;

        // Parse command into arguments
        vector<string> cmd;
        size_t pos = 0, space;
        while ((space = input.find(' ', pos)) != string::npos)
        {
            cmd.push_back(input.substr(pos, space - pos));
            pos = space + 1;
        }
        if (pos < input.size())
        {
            cmd.push_back(input.substr(pos));
        }

        if (cmd.empty())
            continue;

        // Special case: quit
        if (cmd[0] == "quit")
            break;

        // Send command and process response
        if (query(client_fd, cmd) < 0)
        {
            msg("Query failed");
        }
    }
    // Closing the socket
    close(client_fd);

    return 0;
}