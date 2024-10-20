#pragma once

#include <stdint.h>
#include <bits/stdc++.h>

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
    char files[100][256];
} ListResponse;

// Diff request includes list of client files
typedef struct {
    Header header;
    uint32_t fileCount;
    char clientFiles[100][256];
} DiffRequest;

// Diff response includes list of files client does NOT have
typedef struct {
    Header header;
    uint32_t diffCount;
    char diffFiles[100][256];
} DiffResponse;

// Pull request
typedef struct {
    Header header;
    uint32_t fileCount;
    char files[100][256];
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

