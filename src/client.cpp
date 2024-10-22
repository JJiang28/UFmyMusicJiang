#include <iostream>
#include <iomanip>
#include <stdio.h>
#include <fstream> 
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <fstream>
#include <sstream>
#include <openssl/sha.h>
#include "messages.h"
#include <arpa/inet.h>

using namespace std;

#define MAX_BUFFER 4096
#define PORT 8080

const int CHUNK_SIZE = 8192;

void store_client_id(uint32_t client_id) {
    ofstream idFile("client_id.txt"); // store client id
    if (idFile.is_open()) { // input client_id into the txt
        idFile << client_id;
        idFile.close();
    } else {
        perror("Could not store client ID");
    }
}

 
uint32_t load_client_id() {
    ifstream idFile("client_id.txt");
    uint32_t client_id = 0;
    if (idFile.is_open()) {
        idFile >> client_id; // put current id in
        idFile.close();
    }
    return client_id;
}

// function to receive a file from the server and save it to 'client_files/' directory
void receiveFile(int clientSock, const string &filename, uint32_t filesize) {
    FILE *file = fopen(("client_files/" + filename).c_str(), "wb");
    if (!file) {
        perror("Failed to open file");
        return;
    }
    uint32_t bytesReceived = 0;
    char buffer[CHUNK_SIZE];

    // continue receiving until the entire file is received, and for the last file, use remaining bytes since it typically won't be the same size
    while (bytesReceived < filesize) {
        uint32_t remaining = filesize - bytesReceived;
        uint32_t chunkSize;
        if (remaining < CHUNK_SIZE) {
            chunkSize = remaining;
        }
        else {
            chunkSize = CHUNK_SIZE;
        }
        int received = recv(clientSock, buffer, chunkSize, 0);
        if (received <= 0) {
            perror("Failed to receive file chunk");
            break;
        }
        fwrite(buffer, 1, received, file);
        bytesReceived += received;
    }

    fclose(file);
    cout << "File " << filename << " received successfully!" << endl;
}

// helper function to receive a serialized string
string recv_string(int socket) {
    uint32_t len;
    recv(socket, &len, sizeof(len), 0);
    char buffer[len + 1];
    recv(socket, buffer, len, 0); 
    buffer[len] = '\0'; 
    return string(buffer);
}

// function to hash a file using SHA-256 very poggers michael
string hash_file (const string& path) {
    ifstream file("client_files/"+path, ios::binary);
    if (!file) {
        perror("Could not open file!");
        return "";
    }
    SHA256_CTX sha256;
    SHA256_Init(&sha256);

    char buffer[4096];
    while (file.good()) {
        file.read(buffer, sizeof(buffer));
        SHA256_Update(&sha256, buffer, file.gcount()); // hash each chunk
    }
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_Final(hash, &sha256); //make it final

    stringstream hex_hash;
    hex_hash << hex << setfill('0');
    for(int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        hex_hash << hex << setw(2) << (int)hash[i]; // convert to hexa
    }
    return hex_hash.str();
}

vector<string> list_request(int clientSock) {
    Header header;
    header.type = LIST;
    header.size = 0;

    ListRequest req;
    req.header = header;

    // Send LIST request
    if (send(clientSock, &req, sizeof(req), 0) < 0) {
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

    if(combinedLength == 0) {
        cout << "Nothing in Server" << endl;
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
    resp.fileCount = songs.size();
    resp.files = songs;
    return songs;
}

vector<string> diff_request(int clientSock) {
    Header header;
    header.type = DIFF;
    header.size = 0;

    DiffRequest req;
    req.header = header;
    req.fileCount = 0;

    string combinedFiles;
    string hashes;

    if (send(clientSock, &req, sizeof(req), 0) < 0) {
        perror("Could not send DIFF request!");
        return {};
    }

    // crawl local directory
    DIR *dir = opendir("client_files");
    if (dir == NULL) {
        perror("Could not open directory");
        return {};
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && req.fileCount < 100) {
        if (entry->d_type == DT_REG) {
            string fileName = string(entry->d_name);
            //cout << "fileName: " << fileName << endl;
            combinedFiles += fileName + "\n";
            hashes += hash_file(fileName) + "\n";
            req.fileCount++;
            //cout << "this is runnig here" << endl;
        }
    }
    closedir(dir);

    if (req.fileCount == 0) {
        combinedFiles = "dummy_file.txt\n";
        hashes = "dummyhash\n";
        req.fileCount = 1;
    }


    uint32_t combinedLength = combinedFiles.size();
    uint32_t combinedLengthHashes = hashes.size();

    req.clientFiles = combinedFiles;

    //cout << "client: " << combinedLength << endl;
    // Send buffer length and files to server
    if (send(clientSock, &combinedLength, sizeof(combinedLength), 0) < 0) {
        perror("Failed to send combined string length");
        return {};
    }
    if (send(clientSock, combinedFiles.c_str(), combinedLength, 0) < 0) {
        perror("Failed to send combined string");
        return {};
    }
    if (send(clientSock, &combinedLengthHashes, sizeof(combinedLengthHashes), 0) < 0) {
        perror("Failed to send combined hashes length");
        return {};
    }
    if (send(clientSock, hashes.c_str(), combinedLengthHashes, 0) < 0) {
        perror("Failed to send combined hashes");
        return {};
    }

    uint32_t recvLen;
    if (recv(clientSock, &recvLen, sizeof(recvLen), 0) <= 0) {
        perror("Failed to receive combined string length");
        return {};
    }

    if (recvLen == 0) {
        // No differences found
        cout << "No differences found." << endl;
        return {};
    }

    char buffer[recvLen + 1];
    if (recv(clientSock, buffer, recvLen, 0) <= 0) {
        cout << "error is on client side" << endl;
        perror("Failed to receive combined string");
        return {};
    }
   
    buffer[recvLen] = '\0';

    vector<string> songs;
    string cfiles(buffer);
    size_t pos = 0;
    while ((pos = cfiles.find("\n")) != string::npos) {
        songs.push_back(cfiles.substr(0, pos));
        cfiles.erase(0, pos + 1);
    }

    return songs;
}

void pull_request(int clientSock) {
    Header header;
    header.type = PULL;
    header.size = 0;

    PullRequest req;
    req.header = header;
    req.fileCount = 0;

    string combinedFiles;
    string hashes;

    if (send(clientSock, &req, sizeof(req), 0) < 0) {
        perror("Could not send PULL request!");
        return;
    }

    // crawl local directory
    DIR *dir = opendir("client_files");
    if (dir == NULL) {
        perror("Could not open directory");
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && req.fileCount < 100) {
        if (entry->d_type == DT_REG) {
            string fileName = string(entry->d_name);
            combinedFiles += fileName + "\n";
            hashes += hash_file(fileName) + "\n";
            req.fileCount++;
        }
    }
    closedir(dir);

    if (req.fileCount == 0) {
        combinedFiles = "dummy_file.txt\n";
        hashes = "dummyhash\n";
        req.fileCount = 1;
    }


    uint32_t combinedLength = combinedFiles.size();
    uint32_t combinedLengthHashes = hashes.size();

    req.files = combinedFiles;

    // Send buffer length and files to server
    if (send(clientSock, &combinedLength, sizeof(combinedLength), 0) < 0) {
        perror("Failed to send combined string length");
        return;
    }
    if (send(clientSock, combinedFiles.c_str(), combinedLength, 0) < 0) {
        perror("Failed to send combined string");
        return;
    }
    if (send(clientSock, &combinedLengthHashes, sizeof(combinedLengthHashes), 0) < 0) {
        perror("Failed to send combined hashes length");
        return;
    }
    if (send(clientSock, hashes.c_str(), combinedLengthHashes, 0) < 0) {
        perror("Failed to send combined hashes");
        return;
    }

    PullResponse resp;
    if (recv(clientSock, &resp.header, sizeof(resp.header), 0) <= 0) {
        perror("Failed to receive diff header");
        return;
    }

    // cout << "got the header back" << endl;

    uint32_t files_expected;
    if (recv(clientSock, &files_expected, sizeof(files_expected), 0) <= 0) {
        perror("Failed to receive expected file count");
        return;
    }

    if (files_expected == 0) {
        // No differences found
        cout << "No files to pull." << endl;
        return;
    }

    // name, filesize, and content [8192 bytes]
    // cout << files_expected << endl;
    for (uint32_t i = 0; i < files_expected; i++) {
        uint32_t recvLen;
        if (recv(clientSock, &recvLen, sizeof(recvLen), 0) <= 0) {
            perror("Failed to receive filename length");
            return;
        }

        char buffer[recvLen + 1];
        if (recv(clientSock, buffer, recvLen, 0) <= 0) {
            perror("Failed to receive filename");
            return;
        }
        buffer[recvLen] = '\0';
        string filename(buffer);

        uint32_t filesize = 0; 
        if (recv(clientSock, &filesize, sizeof(filesize), 0) <= 0) {
            perror("Failed to receive filesize");
            return;
        }

        receiveFile(clientSock, filename, filesize);
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
    printf("Sent LEAVE request.\n");
}

int main() {
    int client_socket;
    struct sockaddr_in server_addr;

    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0) {
        perror("Socket creation failed");
        exit(1);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        close(client_socket);
        exit(1);
    }

    printf("Connected to the server.\n");

    uint32_t client_id = load_client_id();
    if (send(client_socket, &client_id, sizeof(client_id), 0) < 0) {
        perror("Failed to send client ID");
        close(client_socket);
        exit(1);
    }

    // Receive new client ID if assigned
    if (client_id == 0) {
        if (recv(client_socket, &client_id, sizeof(client_id), 0) <= 0) {
            perror("Failed to receive new client ID");
            close(client_socket);
            exit(1);
        }
        store_client_id(client_id);
    }

    // Receive client history
    uint32_t history_size;
    if (recv(client_socket, &history_size, sizeof(history_size), 0) <= 0) {
        perror("Failed to receive history size");
        close(client_socket);
        exit(1);
    }

    if (history_size > 0) {
        char *history = new char[history_size + 1];
        if (recv(client_socket, history, history_size, 0) <= 0) {
            perror("Failed to receive history content");
            delete[] history;
            close(client_socket);
            exit(1);
        }
        history[history_size] = '\0';
        cout << "Your history:\n" << history << endl;
        delete[] history;
    }

    while (1) {
        string input;
        printf("Enter command (LIST, DIFF, PULL, LEAVE): \n");
        cin >> input;

        vector<string> songs;

        if (input == "LIST") {
            songs = list_request(client_socket);
            if(songs.size() > 0) {
                cout << "Server Files: " << endl;
                for (const string &song : songs) {
                    printf("%s\n", song.c_str());
                }
            }

        }
        else if (input == "DIFF") {
            songs = diff_request(client_socket);
            if(songs.size() > 0){
                cout << "Different Files: " << endl;
                for (const string &song : songs) {
                    printf("%s\n", song.c_str());
                }
            }
        }
        else if (input == "PULL") {
            pull_request(client_socket);
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