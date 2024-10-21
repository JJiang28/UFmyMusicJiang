#include <iostream>
#include <vector>
#include <cstring>
#include <pthread.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <dirent.h>
#include "messages.h"
using namespace std;

#define MAX_BUFFER 4096
#define PORT 8080

void list_songs(int client_socket) {
    ListResponse response;
    response.header.type = LIST;
    response.fileCount = 0;

    string combinedFiles;

    // Open directory and read files
    DIR *dir = opendir("server_files");
    if (dir == NULL) {
        perror("Could not open directory");
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && response.fileCount < 100) {
        if (entry->d_type == DT_REG) {
            // Add each filename to the combined string with a delimiter
            combinedFiles += string(entry->d_name) + "\n";
            response.fileCount++;
        }
    }
    closedir(dir);

    // Send Header
    if (send(client_socket, &response.header, sizeof(response.header), 0) < 0) {
        perror("Failed to send header");
        return;
    }

    // Send the length of the combined string
    uint32_t combinedLength = combinedFiles.size();
    if (send(client_socket, &combinedLength, sizeof(combinedLength), 0) < 0) {
        perror("Failed to send combined string length");
        return;
    }

    // Send the entire concatenated string in one go
    if (send(client_socket, combinedFiles.c_str(), combinedLength, 0) < 0) {
        perror("Failed to send combined string");
        return;
    }
}

void diff_songs() {
    ListResponse response;
    response.header.type = LIST;
}

void *handle_client(void *client_socket) {
    int sock = *((int *)client_socket);
    free(client_socket); 
    char buffer[MAX_BUFFER];
    int bytes_read;

    while ((bytes_read = recv(sock, buffer, sizeof(buffer), 0)) > 0) {
        Header* header = (Header*)buffer;  // Cast the received buffer to the Header structure
        switch (header->type) {
            case LIST:
                list_songs(sock);  // Handle LIST request
                break;
            case DIFF:
                diff_songs();  // Handle DIFF request (to be implemented)
                break;
            case LEAVE:
                close(sock);
                pthread_exit(NULL);  // Handle LEAVE request (exit thread)
            default:
                cout << "Unknown request type." << endl;
                break;
        }
    }

    if (bytes_read == 0) {
        cout << "Client disconnected." << endl;
    } else {
        perror("recv failed");
    }

    close(sock);
    pthread_exit(NULL);
}

int main() {
    int server_socket, *new_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    pthread_t thread_id;

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Socket creation failed");
        exit(1);
    }

    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        close(server_socket);
        exit(1);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (::bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_socket);
        exit(1);
    }

    if (listen(server_socket, 5) < 0) {
        perror("Listen failed");
        close(server_socket);
        exit(1);
    }

    cout << "Server listening on port " << PORT << "..." << endl;

    while (1) {
        int client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_socket < 0) {
            perror("Accept failed");
            close(server_socket);
            exit(1);
        }

        cout << "Client connected." << endl;

        new_sock = (int *)malloc(sizeof(int));
        *new_sock = client_socket;

        if (pthread_create(&thread_id, NULL, handle_client, (void *)new_sock) < 0) {
            perror("Could not create thread");
            free(new_sock);
            close(client_socket);
        }
        pthread_detach(thread_id);
    }

    close(server_socket);
    return 0;
}
