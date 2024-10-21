#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "messages.h"

using namespace std;

#define MAX_BUFFER 4096
#define PORT 8080

// Helper to receive a serialized string
string recv_string(int socket) {
    uint32_t len;
    recv(socket, &len, sizeof(len), 0);  // Receive the length
    char buffer[len + 1];
    recv(socket, buffer, len, 0);  // Then receive the string
    buffer[len] = '\0';  // Null-terminate the string
    return string(buffer);
}

vector<string> list_request(int clientSock) {
    Header header;
    header.type = LIST;
    header.size = 0;

    // Send LIST request
    if (send(clientSock, &header, sizeof(header), 0) < 0) {
        perror("Could not send LIST request!");
        return {};
    }

    // Receive header
    ListResponse resp;
    if (recv(clientSock, &resp.header, sizeof(resp.header), 0) <= 0) {
        perror("Failed to receive header");
        return {};
    }

    // Receive the length of the combined string
    uint32_t combinedLength;
    if (recv(clientSock, &combinedLength, sizeof(combinedLength), 0) <= 0) {
        perror("Failed to receive combined string length");
        return {};
    }

    // Receive the combined string
    char buffer[combinedLength + 1];
    if (recv(clientSock, buffer, combinedLength, 0) <= 0) {
        perror("Failed to receive combined string");
        return {};
    }
    buffer[combinedLength] = '\0';

    vector<string> songs;
    string combinedFiles(buffer);
    size_t pos = 0;
    while ((pos = combinedFiles.find("\n")) != string::npos) {
        songs.push_back(combinedFiles.substr(0, pos));
        combinedFiles.erase(0, pos + 1);
    }

    return songs;
}



void diff_request(int clientSock) {
    Header header;
    header.type = DIFF;
    header.size = 0;

    ListRequest req;
    req.header = header;

    // send DIFF request
    if (send(clientSock, &req, sizeof(req), 0) < 0) {
        perror("Could not send DIFF request!");
    }
}

void leave_request(int clientSock) {
    Header header;
    header.type = LEAVE;
    header.size = 0;

    LeaveRequest req;
    req.header = header;

    if (send(clientSock, &req, sizeof(req), 0) < 0) {
        perror("Could not send LEAVE request!");
    }
    printf("Sent LEAVE request.");
}

int main() {
    int client_socket;
    struct sockaddr_in server_addr;

    // Create socket
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0) {
        perror("Socket creation failed");
        exit(1);
    }

    // Configure server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    // Connect to the server
    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        close(client_socket);
        exit(1);
    }

    printf("Connected to the server.\n");

    while (1) {
        string input;
        printf("Enter command (LIST, DIFF, PULL, LEAVE): \n");
        cin >> input;

        vector<string> songs;

        if (input == "LIST") {
            songs = list_request(client_socket);
            for (const string &song : songs) {
                printf("%s\n", song.c_str());
            }
        }
        else if (input == "DIFF") {
            diff_request(client_socket);
            // Add code to handle DIFF request response
        }
        else if (input == "PULL") {
            // Add code to handle PULL request
        }
        else if (input == "LEAVE") {
            leave_request(client_socket);
            close(client_socket);
            printf("Connection closed.\n");
            return 0;
        }
        else {
            printf("Invalid input!\n");
        }
    }

    return 0;
}
