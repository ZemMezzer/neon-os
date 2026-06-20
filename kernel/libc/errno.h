#pragma once

extern int neon_errno;

#define errno neon_errno

#define ENOENT 2
#define EBADF 9
#define ENOMEM 12
#define EINVAL 22
#define ERANGE 34