#define _GNU_SOURCE

#include <stdlib.h>
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

// List of path replacement pairs.  The first item is the prefix, the
// second the replacement string.  When the prefix ends with a slash,
// it is compared against the beginning of the path; else the whole
// path is compared.  If these are equal, the prefix is replaced with
// the replacement.
static const char *path_map[][2] = {
    { "/etc/os-release", "/tmp/os-release" },
    { "/etc/os-release.d/", "/tmp/os-release.d/" },
};


struct munger {
    int is_prefix;
    char *source_str;
    size_t source_len;
    char *target_str;
    size_t target_len;
    const struct munger *next;
};
static const struct munger *munge_list = NULL;

static int debug = 0;

static __thread char *buffer = NULL;
static __thread size_t buffer_size = 0;


static struct munger *create_munger(const char *source_mem, size_t source_len, const char *target_mem, size_t target_len) {
    struct munger *munger = malloc(sizeof(struct munger));
    if (munger == NULL) {
        goto err;
    }
    memset(munger, 0, sizeof(struct munger));

    if (source_len == 0 || target_len == 0) {
        goto err;
    }

    if (source_mem[source_len - 1] == '/') {
        munger->is_prefix = 1;
        source_len--;
    }
    if (target_mem[target_len - 1] == '/') {
        if (!munger->is_prefix) {
            goto err;
        }
        target_len--;
    }

    munger->source_len = source_len;
    munger->source_str = malloc(source_len + 1);
    if (munger->source_str == NULL) {
        goto err;
    }
    memset(munger->source_str, 0, source_len + 1);
    memcpy(munger->source_str, source_mem, source_len);

    munger->target_len = target_len;
    munger->target_str = malloc(target_len + 1);
    if (munger->target_str == NULL) {
        goto err;
    }
    memset(munger->target_str, 0, target_len + 1);
    memcpy(munger->target_str, target_mem, target_len);

    return munger;

err:
    free(munger);
    return NULL;
}

static struct munger *create_munger_from_strings(const char *source, const char *target) {
    return create_munger(source, strlen(source), target, strlen(target));
}


void __attribute__((constructor)) init() {
    debug = getenv("LIBPATHMUNGER_DEBUG") != NULL;

    struct munger **curr = (struct munger **)&munge_list;
    for (int i = 0; i < (sizeof path_map) / (sizeof *path_map); i++) {
        struct munger *next = create_munger_from_strings(path_map[i][0], path_map[i][1]);
        if (next == NULL) {
            continue;
        }

        if (debug) {
            fprintf(stderr, "Loadded Mapping: (prefix: %c) %s -> %s\n", "NY"[next->is_prefix], next->source_str, next->target_str);
        }
        if (*curr == NULL) {
            *curr = next;
            continue;
        }
        (*curr)->next = next;
        curr = &next;
    }
}


static int match(const char *path, const struct munger *munger) {
    if (strncmp(path, munger->source_str, munger->source_len) != 0) {
        return 0;
    }

    const char next = path[munger->source_len];
    if (next == '\0') {
        return 1;
    }

    if (!munger->is_prefix) {
        return 0;
    }

    return next == '/';
}

static char *get_buffer(size_t min_size) {
    const size_t slack = 63;
    if (min_size == 0) {
        min_size = 1;
    }
    if (min_size > buffer_size) {
        size_t new_size = min_size + slack;
        if (buffer != NULL) {
            free(buffer);
            buffer = NULL;
            buffer_size = 0;
        }
        buffer = malloc(new_size);
        if (buffer != NULL) {
            buffer_size = new_size;
        }
    }
    return buffer;
}

static const char *fix_path(const char *path) {
    if (path == NULL) {
        return NULL;
    }
    for (const struct munger *m = munge_list; m != NULL; m = m->next) {
        int found = match(path, m);
        if (found) {
            char *new_path = get_buffer(strlen(path) - m->source_len + m->target_len);
            if (new_path == NULL) {
                return path;
            }
            strcpy(new_path, m->target_str);
            strcat(new_path, path + m->source_len);
            if (debug) {
                fprintf(stderr, "Mapped Path: %s  ==>  %s\n", path, new_path);
            }
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
        pathname = fix_path(pathname); \
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

