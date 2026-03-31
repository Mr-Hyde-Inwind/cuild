#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

#define DA_INIT_CAP 256
#define da_append(da, item)                                                         \
    do{                                                                             \
        if ((da)->count >= (da)->capacity) {                                        \
            (da)->capacity = (da)->capacity == 0 ? DA_INIT_CAP : (da)->capacity*2;  \
            (da)->items = realloc((da)->items, (da)->capacity*sizeof((da)->items)); \
            assert((da)->items != NULL && "Buy more RAM");                          \
        }                                                                           \
        (da)->items[(da)->count++] = (item);                                        \
    } while(0)

#define da_append_many(da, new_items, n)                                            \
    do {                                                                            \
        if ((da)->count + n > (da)->capacity) {                                     \
            if ((da)->capacity == 0) {                                              \
                (da)->capacity = DA_INIT_CAP;                                       \
            }                                                                       \
            while ((da)->count + n > (da)->capacity) {                              \
                (da)->capacity *= 2;                                                \
            }                                                                       \
            (da)->items = realloc((da)->items, (da)->capacity*sizeof((da)->items)); \
            assert((da)->items != NULL && "Buy more RAM");                          \
        }                                                                           \
        memcpy((da)->items+(da)->count, new_items, n*sizeof((da)->items));          \
        (da)->count += n;                                                           \
    } while(0)

#define cstr_append_null(sb) da_append_many((sb), "", 1)

#define cmd_append(cmd, ...) cmd_append_null(cmd, __VA_ARGS__, NULL)

#ifdef _WIN32
typedef HANDLE Pid;
#else
typedef int Pid;
#endif // _WIN32

#define INVALID_PROC -1

typedef struct {
    const char **items;
    size_t count;
    size_t capacity;
} Cmd;

typedef struct {
    char *items;
    size_t count;
    size_t capacity;
} String_Builder;

void cmd_append_null(Cmd *cmd, ...) {
    va_list args;
    va_start(args, cmd);

    const char *arg = va_arg(args, const char*);
    while (arg != NULL) {
        da_append(cmd, arg);
        arg = va_arg(args, const char*);
    }
    va_end(args);
}

Pid cmd_run_async(Cmd *cmd) {
    int pid = -1;
    if ((pid = fork()) < 0) {
        fprintf(stderr, "[ERROR] Execute cmd failed with error %s\n", strerror(errno));
        return INVALID_PROC;
    }

    if (pid == 0) {
        if (execvp(cmd->items[0], (char * const*)cmd->items) < 0) {
            fprintf(stderr, "[ERROR] Could not exec child process: %s\n", strerror(errno));
            exit(1);
        }
        assert(0 && "unreachable");
    }
    return pid;
}

bool cmd_run_sync(Cmd *cmd) {
    int pid = cmd_run_async(cmd);
    if (pid != INVALID_PROC) {
        int status;
        if (waitpid(pid, &status, 0) < 0) {
            fprintf(stderr, "could not wait on command (pid %d): %s", pid, strerror(errno));
            return false;
        }
        if (WEXITSTATUS(status)) {
            int exit_status = WEXITSTATUS(exit_status);
            if (exit_status != 0) {
                fprintf(stderr, "command exited with exit code %d", exit_status);
            }
            return false;
        }
        if (WIFSIGNALED(status)) {
            fprintf(stderr, "command process was terminated by %s", strsignal(WTERMSIG(status)));
            return false;
        }
    }
    return true;
}

void compiler(Cmd *cmd) {
    cmd_append(cmd, "gcc");
}

void compile_flags(Cmd *cmd) {
    cmd_append(cmd, "-Wall", "-Wextra");
}

void enable_debug(Cmd *cmd) {
    cmd_append(cmd, "-g");
}

int main() {
    Cmd cmd = {0};
    compiler(&cmd);
    compile_flags(&cmd);
    enable_debug(&cmd);
    cmd_append(&cmd, "-o", "build_test", "test.c");
    cmd_run_sync(&cmd);
    return 0;
}
