#include <iostream>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 8080
#define BUFFER_SIZE 1024

using namespace std;

int main()
{
    int client_fd, status;
    struct sockaddr_in server_addr, local_addr, remote_addr;
    socklen_t addr_len = sizeof(local_addr);
    char buffer[BUFFER_SIZE] = {0};
    const char *hello = "Hello from client";

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

    // Sending message to server
    if (send(client_fd, hello, strlen(hello), 0) < 0)
    {
        perror("send failed");
        return -1;
    }
    printf("Hello message sent\n");

    // Receiving response from server
    ssize_t valread = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);
    if (valread < 0)
    {
        perror("recv failed");
        return -1;
    }
    buffer[valread] = '\0'; // Null-terminate the received string
    printf("Server says: %s\n", buffer);

    // Closing the socket
    close(client_fd);

    return 0;
}