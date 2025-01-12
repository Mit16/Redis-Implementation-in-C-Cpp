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
    struct sockaddr_in address;
    char buffer[BUFFER_SIZE] = {0};
    const char *hello = "Hello from client";

    // creating socket
    client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_fd < 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // setting up server address
    address.sin_family = AF_INET;
    address.sin_port = htons(PORT);

    // converting IP address to binary form
    if (inet_pton(AF_INET, "127.0.0.1", &address.sin_addr) <= 0)
    {
        printf(
            "\nInvalid address/ Address not supported \n");
        return -1;
    }

    // connecting to server
    status = connect(client_fd, (struct sockaddr *)&address, sizeof(address));
    if (status < 0)
    {
        printf("Connection Failed");
        return -1;
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