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

enum
{
    TAG_NIL = 0, // nil
    TAG_ERR = 1, // Error code + msg
    TAG_STR = 2, // string
    TAG_INT = 3, // int64
    TAG_DBL = 4, // double
    TAG_ARR = 5, // array
};

// Logging and error handling
static void
msg(const char *msg)
{
#ifdef DEBUG
    cerr << msg << endl;
#endif
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

static int32_t print_response(const uint8_t *data, size_t size)
{
    if (size < 1)
    {
        msg("Bad response");
        return -1;
    }

    switch (data[0])
    {
    case 0: // TAG_NIL
        cout << "(nil)\n";
        return 1;
    case 1: // TAG_ERR
        if (size < 9)
        {
            msg("Bad response");
            return -1;
        }
        {
            int32_t code = 0;
            uint32_t len = 0;
            memcpy(&code, &data[1], 4);
            memcpy(&len, &data[5], 4);
            if (size < 9 + len)
            {
                msg("Bad response");
                return -1;
            }
            printf("(err) %d %.*s\n", code, len, &data[9]);
            return 9 + len;
        }
    case 2: // TAG_STR
        if (size < 5)
        {
            msg("Bad response");
            return -1;
        }
        {
            uint32_t len = 0;
            memcpy(&len, &data[1], 4);
            if (size < 5 + len)
            {
                msg("Bad response");
                return -1;
            }
            printf("(str) %.*s\n", len, &data[5]);
            return 5 + len;
        }
    case 3: // TAG_INT
        if (size < 9)
        {
            msg("Bad response");
            return -1;
        }
        {
            int64_t val = 0;
            memcpy(&val, &data[1], 8);
            printf("(int) %ld\n", val);
            return 9;
        }
    case 4: // TAG_DBL
        if (size < 9)
        {
            msg("Bad response");
            return -1;
        }
        {
            double val = 0;
            memcpy(&val, &data[1], 8);
            printf("(dbl) %g\n", val);
            return 9;
        }
    case 5: // TAG_ARR
        if (size < 5)
        {
            msg("Bad response");
            return -1;
        }
        {
            uint32_t len = 0;
            memcpy(&len, &data[1], 4);
            printf("(arr) len=%u\n", len);
            size_t arr_bytes = 5;
            for (uint32_t i = 0; i < len; ++i)
            {
                int32_t rv = print_response(&data[arr_bytes], size - arr_bytes);
                if (rv < 0)
                {
                    return rv;
                }
                arr_bytes += (size_t)rv;
            }
            printf("(arr) end\n");
            return (int32_t)arr_bytes;
        }
    default:
        msg("Bad response");
        return -1;
    }
}

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

    // Step 3: Print the response
    int32_t rv = print_response(buf.data(), len);
    if (rv > 0 && (uint32_t)rv != len)
    {
        msg("Bad response");
        return -1;
    }
    return rv;
}

// Query the server with a command
static int32_t query(int fd, const vector<string> &cmd)
{
    if (send_request(fd, cmd) < 0)
    {
        msg("Failed to send request");
        return -1;
    }

    // If the command is "quit", expect a response and then close the connection
    if (cmd.size() == 1 && cmd[0] == "quit")
    {
        int32_t rv = read_response(fd);
        if (rv < 0)
        {
            msg("Failed to read response");
            return -1;
        }
        close(fd); // Close the connection
        exit(0);   // Exit the client
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
#ifdef DEBUG
    printf("Connected to the server\n");
#endif
    // Get local address using getsockname()
    if (getsockname(client_fd, (struct sockaddr *)&local_addr, &addr_len) == 0)
    {
#ifdef DEBUG
        printf("Local IP: %s, Port: %d\n",
               inet_ntoa(local_addr.sin_addr),
               ntohs(local_addr.sin_port));
#endif
    }

    // Get server (remote) address using getpeername()
    if (getpeername(client_fd, (struct sockaddr *)&remote_addr, &addr_len) == 0)
    {
#ifdef DEBUG
        printf("Connected to Server - IP: %s, Port: %d\n",
               inet_ntoa(remote_addr.sin_addr),
               ntohs(remote_addr.sin_port));
#endif
    }
    // Handle CLI arguments as single-shot command
    if (argc > 1)
    {
        vector<string> cmd;
        for (int i = 1; i < argc; ++i)
        {
            cmd.emplace_back(argv[i]);
        }

        if (query(client_fd, cmd) < 0)
        {
            msg("Query failed");
        }

        close(client_fd);
        return 0;
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
        size_t pos = 0;
        while (pos < input.size())
        {
            if (input[pos] == ' ')
            {
                pos++;
            }
            else if (input[pos] == '"')
            {
                size_t end = input.find('"', pos + 1);
                if (end == string::npos)
                {
                    msg("Unmatched quote in command");
                    continue;
                }
                cmd.push_back(input.substr(pos + 1, end - pos - 1));
                pos = end + 1;
            }
            else
            {
                size_t end = input.find(' ', pos);
                cmd.push_back(input.substr(pos, end - pos));
                pos = end;
            }
        }

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