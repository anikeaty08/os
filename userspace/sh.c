#include "syscall.h"

static size_t strlen(const char *s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

static int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

static int starts_with(const char *s, const char *prefix) {
    while (*prefix) {
        if (*s++ != *prefix++) {
            return 0;
        }
    }
    return 1;
}

static void puts(const char *s) {
    sys_write(1, s, strlen(s));
}

static int parse_pid(const char *s, long *pid) {
    long value = 0;

    if (!s || !*s) {
        return -1;
    }

    while (*s) {
        if (*s < '0' || *s > '9') {
            return -1;
        }
        value = value * 10 + (*s - '0');
        s++;
    }

    *pid = value;
    return 0;
}

static void print_long(long value) {
    char buf[32];
    int out = 0;

    if (value < 0) {
        buf[out++] = '-';
        value = -value;
    }

    char tmp[24];
    int pos = 0;
    while (value > 0 && pos < (int)sizeof(tmp)) {
        tmp[pos++] = (char)('0' + (value % 10));
        value /= 10;
    }

    if (pos == 0) {
        buf[out++] = '0';
    }
    while (pos > 0) {
        buf[out++] = tmp[--pos];
    }

    sys_write(1, buf, (size_t)out);
}

static void trim_newline(char *s) {
    for (size_t i = 0; s[i]; i++) {
        if (s[i] == '\n' || s[i] == '\r') {
            s[i] = '\0';
            return;
        }
    }
}

static void print_pid(void) {
    long pid = sys_getpid();
    char buf[32];
    int pos = 0;

    if (pid == 0) {
        puts("0\n");
        return;
    }

    char tmp[24];
    while (pid > 0 && pos < (int)sizeof(tmp)) {
        tmp[pos++] = (char)('0' + (pid % 10));
        pid /= 10;
    }

    int out = 0;
    while (pos > 0) {
        buf[out++] = tmp[--pos];
    }
    buf[out++] = '\n';
    sys_write(1, buf, (size_t)out);
}

static void kill_pid(const char *arg) {
    long pid;

    if (parse_pid(arg, &pid) < 0 || pid <= 0) {
        puts("kill: invalid pid\n");
        return;
    }

    if (sys_kill(pid) < 0) {
        puts("kill: failed\n");
        return;
    }

    puts("killed pid ");
    print_long(pid);
    puts("\n");
}

static void wait_pid(const char *arg) {
    long pid;
    int status = 0;

    if (parse_pid(arg, &pid) < 0 || pid <= 0) {
        puts("wait: invalid pid\n");
        return;
    }

    if (sys_wait(pid, &status) < 0) {
        puts("wait: no zombie child\n");
        return;
    }

    puts("pid ");
    print_long(pid);
    puts(" exited ");
    print_long(status);
    puts("\n");
}

static void cat_file(const char *path) {
    char buf[128];
    long fd = sys_open(path);

    if (fd < 0) {
        puts("cat: cannot open file\n");
        return;
    }

    for (;;) {
        long n = sys_read((int)fd, buf, sizeof(buf));
        if (n < 0) {
            puts("cat: read failed\n");
            break;
        }
        if (n == 0) {
            break;
        }
        sys_write(1, buf, (size_t)n);
    }

    sys_close((int)fd);
    puts("\n");
}

static void run_program(const char *path) {
    long pid = sys_spawn(path);

    if (pid < 0) {
        puts("run: failed to spawn program\n");
        return;
    }

    puts("started pid ");
    char buf[32];
    int pos = 0;
    char tmp[24];

    while (pid > 0 && pos < (int)sizeof(tmp)) {
        tmp[pos++] = (char)('0' + (pid % 10));
        pid /= 10;
    }

    int out = 0;
    if (pos == 0) {
        buf[out++] = '0';
    }
    while (pos > 0) {
        buf[out++] = tmp[--pos];
    }
    buf[out++] = '\n';
    sys_write(1, buf, (size_t)out);
}

int main(void) {
    char line[128];

    puts("AstraOS user shell\n");
    puts("type help for commands\n");

    for (;;) {
        puts("user$ ");

        long n = sys_read(0, line, sizeof(line) - 1);
        if (n <= 0) {
            continue;
        }

        line[n] = '\0';
        trim_newline(line);

        if (strcmp(line, "help") == 0) {
            puts("commands: help pid echo cat run kill wait exit yield\n");
        } else if (strcmp(line, "pid") == 0) {
            print_pid();
        } else if (strcmp(line, "yield") == 0) {
            sys_yield();
        } else if (strcmp(line, "echo") == 0) {
            puts("echo from ring 3\n");
        } else if (starts_with(line, "cat ")) {
            cat_file(line + 4);
        } else if (starts_with(line, "run ")) {
            run_program(line + 4);
        } else if (starts_with(line, "kill ")) {
            kill_pid(line + 5);
        } else if (starts_with(line, "wait ")) {
            wait_pid(line + 5);
        } else if (strcmp(line, "exit") == 0) {
            puts("leaving user shell\n");
            return 0;
        } else if (line[0] == '\0') {
            continue;
        } else {
            puts("unknown command\n");
        }
    }
}
