# Client-Server File Synchronization Project!

This project implements a simple client-server application where clients can synchronize files with a server. The server maintains a directory of files, and clients can request lists of files, differences between their local files and the server's files, and pull missing files from the server.


# Requirements
-   **C++ Compiler**: C++ compiler that supports C++11 or later.
-   **OpenSSL Library**:  OpenSSL for SHA-256 hashing.

## Setup

1.  **Clone the Repository**: Clone the repository or download the source code to your local machine.
    
2.  **Create Directories**:
    
    -   **Server Files Directory**: Create a directory named `server_files` and `client_history` in the same directory as your server executable. Populate this directory with the files you want the server to share.
    -   **Client Files Directory**: Create a directory named `client_files` in the same directory as your client executable. The client will use this directory to store synchronized files.

## Compiling the Project

Use the provided `Makefile` to compile both the client and server programs. This command will compile `client.cpp` and `server.cpp`, producing executables named `client` and `server`, respectively. It will also create the necessary directories if they don't exist.

## Executing

Ensure the directories listed in Setup are set up before running executables. 

To start the server, run: ./server

To start the client, run: ./client
