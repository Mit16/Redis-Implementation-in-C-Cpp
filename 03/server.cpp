#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
using namespace std;

#define PORT 8080
#define BUFFER_SIZE 1024

static void do_something(int new_socket)
{
    char buffer[BUFFER_SIZE] = {0};
    ssize_t n;

    // Receiving data from client
    n = recv(new_socket, buffer, sizeof(buffer) - 1, 0);
    if (n < 0)
    {
        perror("recv failed");
        return;
    }
    buffer[n] = '\0'; // Null-terminate the received string
    printf("Received: %s\n", buffer);

    // Sending response to client
    const char *response = "Hello from server";
    if (send(new_socket, response, strlen(response), 0) < 0)
    {
        perror("send failed");
    }
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
            continue; // error
        }

        do_something(new_socket);
        close(new_socket);
    }

    return 0;
}