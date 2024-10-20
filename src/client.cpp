#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "messages.h"

using namespace std;

#define MAX_BUFFER 4096
#define PORT 8080

vector<string> list_request(int clientSock) {
    Header header;
    header.type = LIST;
    header.size = 0;

    ListRequest req;
    req.header = header;

    // send req
    if (send(clientSock, &req, sizeof(req), 0) < 0) {
        perror("Could not send LIST request!");
    }

    // receive req
    ListResponse resp;
    int msglen = recv(clientSock, &resp, sizeof(resp), 0);

    vector<string> songs;
    for (uint32_t i = 0; i < resp.fileCount; i++) {
        songs.push_back(resp.files[i]);
    }

    return songs;
}

// char** get_client_songs() {

// }

void diff_request(int clientSock) {
    Header header;
    header.type = DIFF;
    header.size = 0;

    ListRequest req;
    req.header = header;

    // send req
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
    printf("Sent LEAVE request.")
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

    // Read client text file
    int client_song_count;
    Song *client_songs = read_client_txt("client_music.txt", &client_song_count);
    if (client_songs == NULL) {
        close(client_socket);
        exit(1);  // Error opening the client text file
    }

    while (1) {
        string input;
        printf("Enter command (LIST, DIFF, PULL, LEAVE): \n");
        cin >> input;
        switch (input)
        {
        case "LIST":
            vector<string> songs = list_request(client_socket);
            for (string iter : songs) {
                printf("%s\n", iter);
            }
            break;
        
        case "DIFF":
            break;

        case "PULL":
            break;
        
        case "LEAVE":
            leave_request(client_socket);
            close(client_socket);
            printf("Connection closed.");
            return 0;
        default:
            printf("Invalid input!");
        }
        command[strcspn(command, "\n")] = '\0';  // Remove newline character
    }

    return 0;
}