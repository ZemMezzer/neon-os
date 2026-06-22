#pragma once

#define PACKAGE_ROOT_PATH "0:/.packages"
#define PACKAGE_RUNNER_MAX 8

typedef enum PackageRunnerResult {
    PACKAGE_RUNNER_NOT_APPLICABLE = 0,
    PACKAGE_RUNNER_STARTED = 1,
    PACKAGE_RUNNER_ERROR = -1
} PackageRunnerResult;

typedef PackageRunnerResult (*PackageRunnerFn)(
    const char* package_path,
    int argc,
    char** argv,
    int* out_status
);

typedef enum PackageOpenResult {
    PACKAGE_OPEN_STARTED = 1,
    PACKAGE_OPEN_NO_RUNNER = 0,
    PACKAGE_OPEN_NOT_PACKAGE = -1,
    PACKAGE_OPEN_ERROR = -2
} PackageOpenResult;

int package_runner_register(
    const char* name,
    PackageRunnerFn fn
);

int package_runner_is_package_directory(const char* absolute_path);

PackageOpenResult package_runner_open(
    const char* package_path,
    int argc,
    char** argv,
    int* out_status
);
