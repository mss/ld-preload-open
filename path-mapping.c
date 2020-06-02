#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <dlfcn.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdarg.h>
#include <malloc.h>
/*#include <openat2.h>*/

// List of path pairs. Paths beginning with the first item will be
// translated by replacing the matching part with the second item.
static const char *path_map[][2] = {
    { "/etc/resolv.conf", "/home/poro/resolv.conf" },
};

static __thread char *buffer = NULL;
static __thread int buffer_size = -1;

static int starts_with(const char *str, const char *prefix) {
    return (strncmp(prefix, str, strlen(prefix)) == 0);
}

static char *get_buffer(int min_size) {
    int step = 63;
    if (min_size < 1) {
        min_size = 1;
    }
    if (min_size > buffer_size) {
        if (buffer != NULL) {
            free(buffer);
            buffer = NULL;
            buffer_size = -1;
        }
        buffer = malloc(min_size + step);
        if (buffer != NULL) {
            buffer_size = min_size + step;
        }
    }
    return buffer;
}

static const char *fix_path(const char *path) {
    int count = (sizeof path_map) / (sizeof *path_map); // Array length
    for (int i = 0; i < count; i++) {
        const char *prefix = path_map[i][0];
        const char *replace = path_map[i][1];
        if (starts_with(path, prefix)) {
            const char *rest = path + strlen(prefix);
            char *new_path = get_buffer(strlen(path) + strlen(replace) - strlen(prefix));
            strcpy(new_path, replace);
            strcat(new_path, rest);
            fprintf(stderr, "Mapped Path: %s  ==>  %s\n", path, new_path);
            return new_path;
        }
    }
    return path;
}


// varargs => special case!
typedef int (*orig_open_func)(const char *pathname, int flags, ...);
int open(const char *pathname, int flags, ...) {
    const char *new_path = fix_path(pathname);

    static orig_open_func orig_func = NULL;
    if (orig_func == NULL)
        orig_func = (orig_open_func)dlsym(RTLD_NEXT, "open");

    // If O_CREAT is used to create a file, the file access mode must be given.
    if (flags & O_CREAT) {
        va_list args;
        va_start(args, flags);
        mode_t mode = va_arg(args, mode_t);
        va_end(args);
        return orig_func(new_path, flags, mode);
    } else {
        return orig_func(new_path, flags);
    }
}
int open64(const char *pathname, int flags, ...) {
    const char *new_path = fix_path(pathname);

    static orig_open_func orig_func = NULL;
    if (orig_func == NULL)
        orig_func = (orig_open_func)dlsym(RTLD_NEXT, "open64");

    // If O_CREAT is used to create a file, the file access mode must be given.
    if (flags & O_CREAT) {
        va_list args;
        va_start(args, flags);
        mode_t mode = va_arg(args, mode_t);
        va_end(args);
        return orig_func(new_path, flags, mode);
    } else {
        return orig_func(new_path, flags);
    }
}
typedef int (*orig_openat_func)(int dirfd, const char *pathname, int flags, ...);
int openat(int dirfd, const char *pathname, int flags, ...) {
    const char *new_path = fix_path(pathname);

    static orig_openat_func orig_func = NULL;
    if (orig_func == NULL)
        orig_func = (orig_openat_func)dlsym(RTLD_NEXT, "openat");

    // If O_CREAT is used to create a file, the file access mode must be given.
    if (flags & O_CREAT) {
        va_list args;
        va_start(args, flags);
        mode_t mode = va_arg(args, mode_t);
        va_end(args);
        return orig_func(dirfd, new_path, flags, mode);
    } else {
        return orig_func(dirfd, new_path, flags);
    }
}

// general hook stuff

// can't write C without committing some crimes against code standards
#define DEFINE_HOOK(rett, name, arglist, ...) \
    rett name(__VA_ARGS__) { \
        if (pathname != NULL) pathname = fix_path(pathname); \
        static void *orig_func = NULL; \
        if (orig_func == NULL) orig_func = dlsym(RTLD_NEXT, #name); \
        return ((rett (*)(__VA_ARGS__))(orig_func)) arglist; \
    } \

/*DEFINE_HOOK(int, openat2, (dirfd, pathname, how, size),
        int dirfd, const char* pathname, const struct open_how* how, size_t size)*/

DEFINE_HOOK(int, creat, (pathname, mode), const char* pathname, mode_t mode)

DEFINE_HOOK(int, stat, (pathname, statbuf), const char* pathname, struct stat* statbuf)

DEFINE_HOOK(int, lstat, (pathname, statbuf), const char* pathname, struct stat* statbuf)

// ...

DEFINE_HOOK(FILE*, fopen, (pathname, mode), const char* pathname, const char* mode)

DEFINE_HOOK(FILE*, freopen, (pathname, mode, stream),
        const char* pathname, const char* mode, FILE* stream)

