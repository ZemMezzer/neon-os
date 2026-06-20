#pragma once

/*
    Minimal errno layer for NeonOS' freestanding libc.
    Lua uses errno only to attach a readable cause to I/O and os failures.
*/

extern int errno;

#define EPERM    1
#define ENOENT   2
#define EIO      5
#define EBADF    9
#define EAGAIN   11
#define ENOMEM   12
#define EACCES   13
#define EEXIST   17
#define ENOTDIR  20
#define EISDIR   21
#define EINVAL   22
#define ENFILE   23
#define EMFILE   24
#define ENOSPC   28
#define EROFS    30
#define ENOSYS   38
#define ENAMETOOLONG 36
#define ERANGE 34
