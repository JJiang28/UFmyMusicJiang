#pragma once

#include <cstdint>
#include <string>
#include <vector>
using namespace std;

// Message Types
typedef enum {
    LIST,
    DIFF,
    PULL,
    LEAVE
} Type;

// Header format: Type, size
typedef struct {
    Type type;
    uint32_t size;
} Header;

// List request has no params
typedef struct {
    Header header;
} ListRequest;

// List response should include a list of all song names
typedef struct {
    Header header;
    uint32_t fileCount;
    vector<string> files;
} ListResponse;

// Diff request includes list of client files
typedef struct {
    Header header;
    uint32_t fileCount;
    vector<string> clientFiles;
} DiffRequest;

// Diff response includes list of files the client does NOT have
typedef struct {
    Header header;
    uint32_t diffCount;
    vector<string> diffFiles;
} DiffResponse;

// Pull request
typedef struct {
    Header header;
    uint32_t fileCount;
    vector<string> files;
} PullRequest;

// Each pull response sends a chunk of a requested file
typedef struct {
    Header header;
    char fileName[256];
    uint32_t fileSize;
    char data[1024];
    uint32_t seqNumber;
} PullResponse;

// Leave request has no params
typedef struct {
    Header header;
} LeaveRequest;

// Leave response has a status code: True = ok, False = something wrong
typedef struct {
    Header header;
    bool status;
} LeaveResponse;
