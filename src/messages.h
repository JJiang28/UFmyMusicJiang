#pragma once

#include <cstdint>
#include <string>
#include <vector>
using namespace std;

// Message Types
enum class Type {
    LIST,
    DIFF,
    PULL,
    LEAVE
};

// Header format: Type, size
struct Header {
    Type type;
    uint32_t size;
};

// List request has no params
struct ListRequest {
    Header header;
};

// List response should include a list of all song names
struct ListResponse {
    Header header;
    uint32_t fileCount;
    vector<std::string> files;  
};

// Diff request includes list of client files
struct DiffRequest {
    Header header;
    uint32_t fileCount;
    vector<std::string> clientFiles;  
};

// Diff response includes list of files the client does NOT have
struct DiffResponse {
    Header header;
    uint32_t diffCount;
    vector<std::string> diffFiles;  
};

// Pull request
struct PullRequest {
    Header header;
    uint32_t fileCount;
    vector<std::string> files;
};

// Each pull response sends a chunk of a requested file
struct PullResponse {
    Header header;
    char fileName[256];
    uint32_t fileSize;
    char data[1024];
    uint32_t seqNumber;
};

// Leave request has no params
struct LeaveRequest {
    Header header;
};

// Leave response has a status code: True = ok, False = something wrong
struct LeaveResponse {
    Header header;
    bool status;
};
