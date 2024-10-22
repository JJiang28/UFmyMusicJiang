#include <iostream>
#include <vector>
#include <cstring>
#include <pthread.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <openssl/sha.h>
#include <unordered_set>
#include "messages.h"
using namespace std;

#define MAX_BUFFER 4096
#define PORT 8080

//https://www.geeksforgeeks.org/mutex-lock-for-linux-thread-synchronization/

pthread_mutex_t client_id_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

uint32_t generate_new_client_id() {
    pthread_mutex_lock(&client_id_mutex);

    uint32_t last_client_id = 0;

    // read the last assigned client ID from the file
    ifstream infile("last_client_id.txt");
    if (infile.is_open()) {
        infile >> last_client_id;
        infile.close();
    }
    last_client_id++;

    ofstream outfile("last_client_id.txt");
    if (outfile.is_open()) {
        outfile << last_client_id;
        outfile.close();
    } else {
        perror("Could not write to last_client_id.txt");
    }

    pthread_mutex_unlock(&client_id_mutex);

    return last_client_id;
}

// logging client actions
void log_client_action(uint32_t client_id, const string& action) {
    // preventing race conditions
    pthread_mutex_lock(&log_mutex);
    ofstream log_file;
    log_file.open("client_history/client_" + to_string(client_id) + ".log", ios_base::app);
    if (log_file.is_open()) {
        time_t now = time(0);
        char* dt = ctime(&now);
        log_file << "[" << dt << "] " << action << "\n"; // formatting is a little messed up, idk why the syntax seems fine :(
        log_file.close();
    } else {
        perror("Could not open log file");
    }
    pthread_mutex_unlock(&log_mutex);
}

void send_client_history(int client_socket, uint32_t client_id) {
    pthread_mutex_lock(&log_mutex);
    ifstream log_file("client_history/client_" + to_string(client_id) + ".log");
    if (log_file.is_open()) {
        stringstream buffer;
        buffer << log_file.rdbuf();
        string history = buffer.str();
        uint32_t history_size = history.size();

        // send history size
        if (send(client_socket, &history_size, sizeof(history_size), 0) < 0) {
            perror("Failed to send history size");
            pthread_mutex_unlock(&log_mutex);
            return;
        }

        // send history content
        if (send(client_socket, history.c_str(), history_size, 0) < 0) {
            perror("Failed to send history content");
            pthread_mutex_unlock(&log_mutex);
            return;
        }

        log_file.close();
    } else {
        // No history available
        uint32_t history_size = 0;
        send(client_socket, &history_size, sizeof(history_size), 0);
    }
    pthread_mutex_unlock(&log_mutex);
}


void close_connection(int client_socket) {
    if (close(client_socket) < 0) {
        perror("Failed to close socket");
    } else {
        cout << "Socket closed successfully." << endl;
    }
}

// copy of the stuff in client.cpp
string hash_file (const string& path) {
    ifstream file("server_files/"+path, ios::binary);
    if (!file) {
        perror("Could not open file!");
        return "";
    }
    SHA256_CTX sha256;
    SHA256_Init(&sha256);

    char buffer[4096];
    while (file.good()) {
        file.read(buffer, sizeof(buffer));
        SHA256_Update(&sha256, buffer, file.gcount());
    }
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_Final(hash, &sha256);

    stringstream hex_hash;
    hex_hash << hex << setfill('0');
    for(int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        hex_hash << hex << setw(2) << (int)hash[i];
    }
    return hex_hash.str();
}

void list_songs(int client_socket) {
    cout << "runs list songs first" << endl;
    ListResponse response;
    response.header.type = LIST;
    response.fileCount = 0;

    string combinedFiles;

    DIR *dir = opendir("server_files");
    if (dir == NULL) {
        perror("Could not open directory");
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && response.fileCount < 100) {
        if (entry->d_type == DT_REG) {
            string fileName = string(entry->d_name);
            //cout << fileName << endl;
            combinedFiles += string(entry->d_name) + "\n";
            response.fileCount++;
        }
        // cout << "riuns jerherw " << endl;
    }
    closedir(dir);

    if (send(client_socket, &response.header, sizeof(response.header), 0) < 0) {
        perror("Failed to send header");
        return;
    }

    // send the length of the combined string
    uint32_t combinedLength = combinedFiles.size();
    if (send(client_socket, &combinedLength, sizeof(combinedLength), 0) < 0) {
        perror("Failed to send combined string length");
        return;
    }

    // send the entire concatenated string in one go
    if (send(client_socket, combinedFiles.c_str(), combinedLength, 0) < 0) {
        perror("Failed to send combined string");
        return;
    }
}

void diff_songs(int client_sock) {
    cout << "runs diff songs first" << endl;

    DiffResponse resp;
    resp.header.type = DIFF;
    resp.diffCount = 0;

    // if (recv(clientSock, &resp.header, sizeof(resp.header), 0) <= 0) {
    //     perror("Failed to receive header");
    //     return {};
    // }
    
    uint32_t combinedLength;
    
    if (recv(client_sock, &combinedLength, sizeof(combinedLength), 0) <= 0) {
        perror("Failed to receive combined string length");
        return;
    }

    //cout << "server: " << combinedLength << endl;

    char buffer[combinedLength + 1];
    if (recv(client_sock, buffer, combinedLength, 0) <= 0) {
        cout << "error in receiving the concat string in server" << endl;
        perror("Failed to receive combined string");
        return;
    }
    //cout << "runs past here" << endl;
    buffer[combinedLength] = '\0';

    uint32_t combinedLengthHashes;
    if (recv(client_sock, &combinedLengthHashes, sizeof(combinedLengthHashes), 0) <= 0) {
        perror("Failed to receive combined string length");
        return;
    }

    char bufferHash[combinedLengthHashes + 1];
    if (recv(client_sock, bufferHash, combinedLengthHashes, 0) <= 0) {
        perror("Failed to receive combined string");
        return;
    }

    vector<string> songs;
    string combinedFiles(buffer);
    size_t pos = 0;
    while ((pos = combinedFiles.find("\n")) != string::npos) {
        songs.push_back(combinedFiles.substr(0, pos));
        combinedFiles.erase(0, pos + 1);
    }

    vector<string> hashes;
    string combinedHashes(bufferHash);
    pos = 0;
    while ((pos = combinedHashes.find("\n")) != string::npos) {
        hashes.push_back(combinedHashes.substr(0, pos));
        combinedHashes.erase(0, pos + 1);
    }

    if (songs.size() != hashes.size()) {
        perror("Mismatch between number of songs and hashes");
        return;
    }

    unordered_set<string> client_hashes(hashes.begin(), hashes.end());

    DIR *dir = opendir("server_files");
    if (dir == NULL) {
        perror("Could not open directory");
        return;
    }

    string combinedDiffFiles;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            string fileName = string(entry->d_name);
            string server_hash = hash_file(fileName);
            if (client_hashes.find(server_hash) == client_hashes.end()) {
                // Client does not have this content
                combinedDiffFiles += fileName + "\n";
            }
        }
    }
    closedir(dir);

    // if (send(client_sock, &resp.header, sizeof(resp.header), 0) < 0) {
    //     perror("Failed to send header");
    //     return;
    // }

    uint32_t combinedLengthS = combinedDiffFiles.size();
    resp.diffCount = combinedLengthS;
    resp.diffFiles = combinedDiffFiles.c_str();
    if (send(client_sock, &combinedLengthS, sizeof(combinedLengthS), 0) < 0) {
        perror("Failed to send combined string length");
        return;
    }
    if(combinedLengthS > 0){
        if (send(client_sock, combinedDiffFiles.c_str(), combinedLengthS, 0) < 0) {
        perror("Failed to send combined string");
        return;
        }
    }
}

void send_file(int client_socket, const string& filepath) {
    ifstream file("server_files/" + filepath, ios::binary | ios::ate);
    if (!file.is_open()) {
        perror("Could not open file!");
        return;
    }

    uint32_t filepath_len = filepath.size();
    if (send(client_socket, &filepath_len, sizeof(filepath_len), 0) < 0) {
        perror("Failed to send filepath length");
        file.close();
        return;
    }

    if (send(client_socket, filepath.c_str(), filepath_len, 0) < 0) {
        perror("Failed to send filepath");
        file.close();
        return;
    }

    uint32_t file_size = file.tellg();
    file.seekg(0, ios::beg);

    if (send(client_socket, &file_size, sizeof(file_size), 0) < 0) {
        perror("Could not send file size");
        file.close();
        return;
    }

    // Send file content
    char buffer[8192];
    while (file.good()) {
        file.read(buffer, sizeof(buffer));
        streamsize bytes_read = file.gcount();

        if (send(client_socket, buffer, bytes_read, 0) < 0) {
            perror("Error: Sending file failed");
            break;
        }
    }
    file.close();
}


void pull_songs(int client_sock) {
    PullResponse resp;
    resp.header.type = DIFF;
    resp.fileCount = 0;

    // if (recv(clientSock, &resp.header, sizeof(resp.header), 0) <= 0) {
    //     perror("Failed to receive header");
    //     return {};
    // }
    
    uint32_t combinedLength;
    
    if (recv(client_sock, &combinedLength, sizeof(combinedLength), 0) <= 0) {
        perror("Failed to receive combined string length");
        return;
    }

    //cout << "server: " << combinedLength << endl;

    char buffer[combinedLength + 1];
    if (recv(client_sock, buffer, combinedLength, 0) <= 0) {
        cout << "error in receiving the concat string in server" << endl;
        perror("Failed to receive combined string");
        return;
    }
    //cout << "runs past here" << endl;
    buffer[combinedLength] = '\0';

    uint32_t combinedLengthHashes;
    if (recv(client_sock, &combinedLengthHashes, sizeof(combinedLengthHashes), 0) <= 0) {
        perror("Failed to receive combined string length");
        return;
    }

    char bufferHash[combinedLengthHashes + 1];
    if (recv(client_sock, bufferHash, combinedLengthHashes, 0) <= 0) {
        perror("Failed to receive combined string");
        return;
    }

    vector<string> songs;
    string combinedFiles(buffer);
    size_t pos = 0;
    while ((pos = combinedFiles.find("\n")) != string::npos) {
        songs.push_back(combinedFiles.substr(0, pos));
        combinedFiles.erase(0, pos + 1);
    }

    vector<string> hashes;
    string combinedHashes(bufferHash);
    pos = 0;
    while ((pos = combinedHashes.find("\n")) != string::npos) {
        hashes.push_back(combinedHashes.substr(0, pos));
        combinedHashes.erase(0, pos + 1);
    }

    if (songs.size() != hashes.size()) {
        perror("Mismatch between number of songs and hashes");
        return;
    }

    unordered_set<string> client_hashes(hashes.begin(), hashes.end());

    DIR *dir = opendir("server_files");
    if (dir == NULL) {
        perror("Could not open directory");
        return;
    }

    // cout << "me when i serve" << endl;

    if (send(client_sock, &resp.header, sizeof(resp.header), 0) < 0) {
        perror("Error: Sending file failed");
        return;
    }

    cout << "server sent response header" << endl;

    // crawl server directory
    vector<string> files_to_send;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            string fileName = string(entry->d_name);
            string server_hash = hash_file(fileName);

            if (client_hashes.find(server_hash) == client_hashes.end()) {
                // client does not have this content
                files_to_send.push_back(fileName);
            }
        }
    }
    closedir(dir);

    // send client number of files to expect
    uint32_t numfiles = files_to_send.size();
    resp.fileCount = numfiles;
    resp.clientFiles = files_to_send;
    if (send(client_sock, &numfiles, sizeof(numfiles), 0) < 0) {
        perror("Failed to send number of files");
        return;
    }
    if(numfiles > 0) {
        for (uint32_t i = 0; i < numfiles; i++) {
        send_file(client_sock, files_to_send[i]);
        }
    }
}

void *handle_client(void *client_socket) {
    int sock = *((int *)client_socket);
    free(client_socket); 
    char buffer[MAX_BUFFER];
    int bytes_read;
    uint32_t client_id;
    if (recv(sock, &client_id, sizeof(client_id), 0) <= 0) {
        perror("Failed to receive client ID");
        close(sock);
        pthread_exit(NULL);
    }

    if (client_id == 0) {
        client_id = generate_new_client_id();
        if (send(sock, &client_id, sizeof(client_id), 0) < 0) {
            perror("Failed to send new client ID");
            close(sock);
            pthread_exit(NULL);
        }
    }

    send_client_history(sock, client_id);

    while ((bytes_read = recv(sock, buffer, sizeof(buffer), 0)) > 0) {
        Header* header = (Header*)buffer;
        switch (header->type) {
            case LIST:
                list_songs(sock);
                log_client_action(client_id, "Requested LIST");
                break;
            case DIFF:
                cout << "runs here" << endl;
                diff_songs(sock);
                log_client_action(client_id, "Requested DIFF");
                break;
            case PULL:
                pull_songs(sock);
                log_client_action(client_id, "Requested PULL");
                break;
            case LEAVE:
                close_connection(sock);
                log_client_action(client_id, "Disconnected");
                pthread_exit(NULL);
            default:
                cout << "Unknown request type." << endl;
                log_client_action(client_id, "Sent unknown request");
                break;
        }
    }

    if (bytes_read == 0) {
        cout << "Client disconnected." << endl;
        log_client_action(client_id, "Client disconnected");
    } else {
        perror("recv failed");
        log_client_action(client_id, "Receive failed");
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
